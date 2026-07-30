// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_skills_provider.h"

#include <string_view>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "components/skills/internal/skill_parser.rs.h"
#include "components/skills/public/skills_prefs.h"
#include "crypto/hash.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace skills {

namespace {

constexpr size_t kMaxSkillDownloadSize = 100 * 1024;
constexpr base::TimeDelta kSkillFetchTimeout = base::Seconds(5);
constexpr int kMaxNetworkRetries = 3;
constexpr char kDefaultEnterpriseSkillIcon[] = "💼";

bool IsFieldValid(std::string_view field, size_t max_length) {
  return !field.empty() && field.length() <= max_length;
}

constexpr char kBaseValidationError[] =
    "Validation failed for enterprise skill (Hash: ";

EnterpriseSkillValidationResult ValidateSkillMetadata(
    std::string_view expected_hash,
    std::string_view name,
    std::string_view description,
    std::string_view prompt) {
  if (!IsFieldValid(name, Skill::kMaxNameLength)) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << kBaseValidationError << expected_hash
        << "). Reason: Invalid name length.";
    return EnterpriseSkillValidationResult::kInvalidName;
  }
  if (!IsFieldValid(description, Skill::kMaxDescriptionLength)) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << kBaseValidationError << expected_hash
        << "). Reason: Invalid description length.";
    return EnterpriseSkillValidationResult::kInvalidDescription;
  }
  if (!IsFieldValid(prompt, Skill::kMaxPromptLength)) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << kBaseValidationError << expected_hash
        << "). Reason: Invalid prompt length.";
    return EnterpriseSkillValidationResult::kInvalidPrompt;
  }
  return EnterpriseSkillValidationResult::kSuccess;
}

bool IsHashValid(std::string_view actual_hash_hex,
                 std::string_view expected_hash) {
  if (!base::EqualsCaseInsensitiveASCII(actual_hash_hex, expected_hash)) {
    // TODO(b/533517209): Record Enterprise.Skills.ValidationResult
    // (kHashMismatch).
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Enterprise skill hash mismatch. "
        << "Expected: " << expected_hash << ", Actual: " << actual_hash_hex;
    return false;
  }
  return true;
}

}  // namespace

EnterpriseSkillsProvider::EnterpriseSkillsProvider(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      url_loader_factory_(std::move(url_loader_factory)) {
  if (pref_service_ && url_loader_factory_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        prefs::kEnterprisePublishedSkills,
        base::BindRepeating(&EnterpriseSkillsProvider::OnPolicyPrefChanged,
                            base::Unretained(this)));

    // Initial fetch if policy is already set.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&EnterpriseSkillsProvider::OnPolicyPrefChanged,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}
EnterpriseSkillsProvider::~EnterpriseSkillsProvider() = default;

base::CallbackListSubscription
EnterpriseSkillsProvider::RegisterSkillsChangedCallback(
    SkillsChangedCallback callback) {
  return on_skills_changed_callbacks_.Add(std::move(callback));
}

const std::vector<std::unique_ptr<Skill>>& EnterpriseSkillsProvider::GetSkills()
    const {
  return skills_;
}

void EnterpriseSkillsProvider::RefreshSkills() {
  FetchSkillsFromUrls();
}

void EnterpriseSkillsProvider::OnPolicyPrefChanged() {
  FetchSkillsFromUrls();
}

void EnterpriseSkillsProvider::FetchSkillsFromUrls() {
  barrier_closure_.Cancel();
  url_loaders_.clear();
  pending_skills_.clear();

  if (!pref_service_) {
    return;
  }
  const base::ListValue& skills_list =
      pref_service_->GetList(prefs::kEnterprisePublishedSkills);

  std::vector<std::pair<GURL, std::string>> skills_to_fetch;

  for (const base::Value& skill_value : skills_list) {
    CHECK(skill_value.is_dict());
    const base::DictValue& dict = skill_value.GetDict();
    const std::string* url_str = dict.FindString("url");
    const std::string* hash_str = dict.FindString("hash");
    CHECK(url_str && hash_str);

    GURL url(*url_str);

    skills_to_fetch.emplace_back(url, *hash_str);
  }

  if (skills_to_fetch.empty()) {
    OnAllFetchesComplete();
    return;
  }

  barrier_closure_.Reset(base::BarrierClosure(
      skills_to_fetch.size(),
      base::BindOnce(&EnterpriseSkillsProvider::OnAllFetchesComplete,
                     weak_ptr_factory_.GetWeakPtr())));

  for (const auto& fetch_info : skills_to_fetch) {
    const GURL& url = fetch_info.first;
    const std::string& expected_hash = fetch_info.second;

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("enterprise_skills_fetcher", R"(
        semantics {
          sender: "Enterprise Skills Fetcher"
          description:
            "Fetches corporate skills definition files specified by the "
            "administrator via the EnterprisePublishedSkills policy."
          trigger: "Profile startup or when the policy changes."
          data: "No user data is sent. Only the request for the URL is sent."
          destination: OTHER
          internal {
            contacts {
               email: "cbe-productivity-eng-team@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2026-07-16"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings but is controlled "
            "by the EnterprisePublishedSkills policy."
          chrome_policy {
            EnterprisePublishedSkills {
              EnterprisePublishedSkills: "[]"
            }
          }
        })");

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    auto url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);

    url_loader->SetTimeoutDuration(kSkillFetchTimeout);
    url_loader->SetRetryOptions(
        kMaxNetworkRetries,
        network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
            network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);

    network::SimpleURLLoader* loader_ptr = url_loader.get();
    loader_ptr->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&EnterpriseSkillsProvider::OnURLLoadComplete,
                       weak_ptr_factory_.GetWeakPtr(), loader_ptr,
                       expected_hash, barrier_closure_.callback()),
        kMaxSkillDownloadSize);

    url_loaders_.emplace_back(std::move(url_loader));
  }
}

std::unique_ptr<Skill> EnterpriseSkillsProvider::ParseAndValidateSkill(
    std::string_view expected_hash,
    std::string_view response_body) const {
  // Validate SHA256.
  std::string actual_hash_hex =
      base::HexEncode(crypto::hash::Sha256(response_body));

  if (!IsHashValid(actual_hash_hex, expected_hash)) {
    return nullptr;
  }

  ffi::SkillParseResult parsed =
      ffi::parse_skill_yaml_frontmatter(std::string(response_body));
  if (!parsed.success) {
    // TODO(b/533517209): Record Enterprise.Skills.ValidationResult
    // (kInvalidFormat).
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Enterprise skill validation failed for hash: " << expected_hash;
    return nullptr;
  }

  std::string parsed_name = std::string(parsed.name);
  std::string parsed_desc = std::string(parsed.description);
  std::string prompt_content = std::string(parsed.prompt);

  EnterpriseSkillValidationResult validation_status = ValidateSkillMetadata(
      expected_hash, parsed_name, parsed_desc, prompt_content);

  if (validation_status != EnterpriseSkillValidationResult::kSuccess) {
    // TODO(b/533517209): Record Enterprise.Skills.ValidationResult (using
    // validation_status).
    return nullptr;
  }

  auto new_skill = std::make_unique<Skill>();
  new_skill->id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  new_skill->name = parsed_name;
  new_skill->description = parsed_desc;
  new_skill->icon = kDefaultEnterpriseSkillIcon;
  new_skill->prompt = prompt_content;
  // TODO(b/536902210): Add SKILL_SOURCE_ENTERPRISE to skills.proto.
  // For now, these are marked UNKNOWN since they are distinct from user or 1P
  // skills. The frontend UI can rely on this downstream.
  new_skill->source = sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN;

  // TODO(b/533517209): Record Enterprise.Skills.ValidationResult (kSuccess).
  return new_skill;
}

void EnterpriseSkillsProvider::OnURLLoadComplete(
    network::SimpleURLLoader* source,
    const std::string& expected_hash,
    base::RepeatingClosure barrier_closure,
    std::optional<std::string> response_body) {
  // TODO(b/533517209): Record Enterprise.Skills.FetchResult and
  // Enterprise.Skills.FetchDelta.
  if (response_body) {
    auto new_skill = ParseAndValidateSkill(expected_hash, *response_body);
    if (new_skill) {
      pending_skills_.emplace_back(std::move(new_skill));
    }
    // Validation failures natively log specific errors inside
    // ParseAndValidateSkill.
  } else {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Failed to download enterprise skill for hash: " << expected_hash;
  }

  std::erase_if(url_loaders_, [source](const auto& loader) {
    return loader.get() == source;
  });
  barrier_closure.Run();
}

void EnterpriseSkillsProvider::OnAllFetchesComplete() {
  // TODO(b/533517209): Record Enterprise.Skills.Count.
  skills_ = std::move(pending_skills_);
  pending_skills_.clear();
  NotifyObservers();
}

void EnterpriseSkillsProvider::NotifyObservers() {
  on_skills_changed_callbacks_.Notify();
}

}  // namespace skills

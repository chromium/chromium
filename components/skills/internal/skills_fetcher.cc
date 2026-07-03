// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_fetcher.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_metrics.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace skills {

namespace {

constexpr net::NetworkTrafficAnnotationTag kSkillsFetcherNetworkTag =
    net::DefineNetworkTrafficAnnotation("skills_fetcher", R"(
      semantics {
        sender: "Skills Service"
        description:
          "Downloads a list of first-party agent prompts from the Skills API. "
          "These skills are displayed on chrome://skills, allowing users to "
          "browse a list of first-party skills."
        trigger:
          "Triggered on chrome://skills page load or attempt "
          "to save a skill."
        data: "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
            type: ACCESS_TOKEN
        }
        internal {
            contacts {
              email: "chrome-skills-eng-team@google.com"
            }
        }
        last_reviewed: "2026-05-08"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled by turning off the Skills "
          "feature in chrome://flags/#skills."
        chrome_policy {
          GeminiSettings {
            GeminiSettings: 1
          }
        }
      })");

bool ParseSkillsListFromJson(const std::string& json_str,
                             skills::proto::SkillsList* skills_list) {
  std::optional<base::DictValue> result = base::JSONReader::ReadDict(
      json_str, base::JSONParserOptions::JSON_PARSE_RFC);
  if (!result.has_value()) {
    return false;
  }
  const base::DictValue& root_dict = *result;

  // Parse skills
  const base::ListValue* skills_val = root_dict.FindList("skills");
  if (skills_val) {
    for (const auto& skill_val : *skills_val) {
      if (!skill_val.is_dict()) {
        continue;
      }
      const base::DictValue& skill_dict = skill_val.GetDict();
      auto* skill = skills_list->add_skills();
      if (const std::string* id = skill_dict.FindString("id")) {
        skill->set_id(*id);
      }
      if (const std::string* name = skill_dict.FindString("name")) {
        skill->set_name(*name);
      }
      if (const std::string* category = skill_dict.FindString("category")) {
        skill->set_category(*category);
      }
      if (const std::string* icon = skill_dict.FindString("icon")) {
        skill->set_icon(*icon);
      }
      if (const std::string* prompt = skill_dict.FindString("prompt")) {
        skill->set_prompt(*prompt);
      }
      if (const std::string* description =
              skill_dict.FindString("description")) {
        skill->set_description(*description);
      }
      if (const std::string* image_url = skill_dict.FindString("imageUrl")) {
        skill->set_image_url(*image_url);
      } else if (const std::string* image_url_snake =
                     skill_dict.FindString("image_url")) {
        skill->set_image_url(*image_url_snake);
      }
      if (const std::string* curated_by = skill_dict.FindString("curatedBy")) {
        skill->set_curated_by(*curated_by);
      } else if (const std::string* curated_by_snake =
                     skill_dict.FindString("curated_by")) {
        skill->set_curated_by(*curated_by_snake);
      }
    }
  }

  // Parse topics_info_list
  const base::ListValue* topics_val = root_dict.FindList("topicsInfoList");
  if (!topics_val) {
    topics_val = root_dict.FindList("topics_info_list");
  }
  if (topics_val) {
    for (const auto& topic_val : *topics_val) {
      if (!topic_val.is_dict()) {
        continue;
      }
      const base::DictValue& topic_dict = topic_val.GetDict();
      auto* topic = skills_list->add_topics_info_list();
      if (const std::string* category_name =
              topic_dict.FindString("categoryName")) {
        topic->set_category_name(*category_name);
      } else if (const std::string* category_name_snake =
                     topic_dict.FindString("category_name")) {
        topic->set_category_name(*category_name_snake);
      }
      if (const std::string* display_name =
              topic_dict.FindString("displayName")) {
        topic->set_display_name(*display_name);
      } else if (const std::string* display_name_snake =
                     topic_dict.FindString("display_name")) {
        topic->set_display_name(*display_name_snake);
      }
    }
  }

  return true;
}

}  // namespace

SkillsFetcher::SkillsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {}

SkillsFetcher::~SkillsFetcher() = default;

void SkillsFetcher::FetchDiscoverySkills(OnFetchCompleteCallback callback) {
  if (endpoint_fetcher_) {
    // Request already in progress.
    return;
  }

  GURL url(features::kSkillsServiceApiUrl.Get());

  endpoint_fetcher_ = std::make_unique<endpoint_fetcher::EndpointFetcher>(
      url_loader_factory_, identity_manager_,
      endpoint_fetcher::EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kGet, kSkillsFetcherNetworkTag)
          .SetUrl(url)
          .SetAuthType(endpoint_fetcher::AuthType::OAUTH)
          .SetOAuthConsumerId(signin::OAuthConsumerId::kSkillsService)
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .Build());

  endpoint_fetcher_->Fetch(base::BindOnce(&SkillsFetcher::OnResponseFetched,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(callback)));
}

void SkillsFetcher::OnResponseFetched(
    OnFetchCompleteCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  endpoint_fetcher_.reset();

  if (!response || response->http_status_code != net::HTTP_OK) {
    RecordSkillsFetchResult(SkillsFetchResult::kNetworkError);
    std::move(callback).Run(nullptr);
    return;
  }

  if (response->response.empty()) {
    RecordSkillsFetchResult(SkillsFetchResult::kEmptyResponseBody);
    std::move(callback).Run(nullptr);
    return;
  }

  skills::proto::SkillsList skills_list;
  if (!ParseSkillsListFromJson(response->response, &skills_list)) {
    RecordSkillsFetchResult(SkillsFetchResult::kProtoParseFailure);
    std::move(callback).Run(nullptr);
    return;
  }

  auto first_party_skill_data = std::make_unique<FirstPartySkillData>();

  // If a skill curated by field is not set, default to Chrome
  for (auto& skill : *skills_list.mutable_skills()) {
    if (!skill.has_curated_by() || skill.curated_by().empty()) {
      skill.set_curated_by("Chrome");
    }
  }

  first_party_skill_data->skills_list.insert(
      first_party_skill_data->skills_list.end(),
      std::make_move_iterator(skills_list.mutable_skills()->begin()),
      std::make_move_iterator(skills_list.mutable_skills()->end()));

  first_party_skill_data->topics_info_list.insert(
      first_party_skill_data->topics_info_list.end(),
      std::make_move_iterator(skills_list.mutable_topics_info_list()->begin()),
      std::make_move_iterator(skills_list.mutable_topics_info_list()->end()));

  RecordSkillsFetchResult(SkillsFetchResult::kSuccess);
  std::move(callback).Run(std::move(first_party_skill_data));
}

}  // namespace skills

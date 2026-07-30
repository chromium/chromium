// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_skills_provider.h"

#include <string_view>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/skills/internal/skill_parser.rs.h"
#include "crypto/hash.h"

namespace skills {

namespace {

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

EnterpriseSkillsProvider::EnterpriseSkillsProvider() = default;
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
  // TODO(b/536902210): Implement downstream URL fetching and wiring for Policy
  // observed changes.
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

}  // namespace skills

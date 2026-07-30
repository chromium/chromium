// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_ENTERPRISE_SKILLS_PROVIDER_H_
#define COMPONENTS_SKILLS_INTERNAL_ENTERPRISE_SKILLS_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_provider.h"

namespace skills {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(b/533517209): Wire this up to Enterprise.Skills.ValidationResult in
// histograms/enums.xml.
enum class EnterpriseSkillValidationResult {
  kSuccess = 0,
  kHashMismatch = 1,
  kInvalidFormat = 2,
  kInvalidName = 3,
  kInvalidDescription = 4,
  kInvalidPrompt = 5,
  kMaxValue = kInvalidPrompt,
};

class EnterpriseSkillsProvider : public SkillsProvider {
 public:
  EnterpriseSkillsProvider();
  ~EnterpriseSkillsProvider() override;

  EnterpriseSkillsProvider(const EnterpriseSkillsProvider&) = delete;
  EnterpriseSkillsProvider& operator=(const EnterpriseSkillsProvider&) = delete;

  // SkillsProvider implementation:
  base::CallbackListSubscription RegisterSkillsChangedCallback(
      SkillsChangedCallback callback) override;
  const std::vector<std::unique_ptr<Skill>>& GetSkills() const override;
  void RefreshSkills() override;

  // Parses and validates a raw YAML skill response. Returns nullptr if
  // validation fails.
  std::unique_ptr<Skill> ParseAndValidateSkill(
      std::string_view expected_hash,
      std::string_view response_body) const;

 private:
  // The cached list of successfully fetched and validated enterprise skills.
  std::vector<std::unique_ptr<Skill>> skills_;

  base::RepeatingCallbackList<void()> on_skills_changed_callbacks_;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_ENTERPRISE_SKILLS_PROVIDER_H_

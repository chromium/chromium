// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_ENTERPRISE_SKILLS_PROVIDER_H_
#define COMPONENTS_SKILLS_INTERNAL_ENTERPRISE_SKILLS_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace network {
class SimpleURLLoader;
}

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
  EnterpriseSkillsProvider(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
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
  // Callback when prefs::kEnterprisePublishedSkills changes.
  void OnPolicyPrefChanged();

  // Triggers the background network fetches using SimpleURLLoader.
  void FetchSkillsFromUrls();

  void OnURLLoadComplete(network::SimpleURLLoader* source,
                         const std::string& expected_hash,
                         base::RepeatingClosure barrier_closure,
                         std::optional<std::string> response_body);

  // Called when all background URL fetches have completed.
  void OnAllFetchesComplete();

  // Notifies all registered observers that skills_ has been updated.
  void NotifyObservers();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_registrar_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Active loaders for background fetching.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  // Accumulates skills during a fetch operation before replacing skills_.
  std::vector<std::unique_ptr<Skill>> pending_skills_;
  // The cached list of successfully fetched and validated enterprise skills.
  std::vector<std::unique_ptr<Skill>> skills_;

  base::RepeatingCallbackList<void()> on_skills_changed_callbacks_;
  base::CancelableRepeatingClosure barrier_closure_;
  base::WeakPtrFactory<EnterpriseSkillsProvider> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_ENTERPRISE_SKILLS_PROVIDER_H_

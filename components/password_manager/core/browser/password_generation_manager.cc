// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include <map>
#include <utility>

#include "base/callback.h"
#include "base/time/default_clock.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {
namespace {

using autofill::PasswordForm;
using metrics_util::GenerationPresaveConflict;
using metrics_util::LogGenerationPresaveConflict;

std::vector<PasswordForm> DeepCopyVector(
    const std::vector<const PasswordForm*>& forms) {
  std::vector<PasswordForm> result;
  result.reserve(forms.size());
  for (const PasswordForm* form : forms)
    result.emplace_back(*form);
  return result;
}

// Implementation of the UI model for "Update password?" bubble in case there is
// a conflict in generation.
class PasswordDataForUI : public PasswordFormManagerForUI {
 public:
  PasswordDataForUI(PasswordForm pending_form,
                    const std::vector<const PasswordForm*>& matches,
                    const std::vector<const PasswordForm*>& federated,
                    base::RepeatingCallback<void(bool, const PasswordForm&)>
                        bubble_interaction);
  ~PasswordDataForUI() override = default;
  PasswordDataForUI(const PasswordDataForUI&) = delete;
  PasswordDataForUI& operator=(const PasswordDataForUI&) = delete;

  // PasswordFormManagerForUI:
  const GURL& GetOrigin() const override;
  const std::vector<const PasswordForm*>& GetBestMatches() const override;
  std::vector<const PasswordForm*> GetFederatedMatches() const override;
  const PasswordForm& GetPendingCredentials() const override;
  metrics_util::CredentialSourceType GetCredentialSource() const override;
  PasswordFormMetricsRecorder* GetMetricsRecorder() override;
  base::span<const InteractionsStats> GetInteractionsStats() const override;
  bool IsBlacklisted() const override;
  void Save() override;
  void Update(const PasswordForm& credentials_to_update) override;
  void OnUpdateUsernameFromPrompt(const base::string16& new_username) override;
  void OnUpdatePasswordFromPrompt(const base::string16& new_password) override;
  void OnNopeUpdateClicked() override;
  void OnNeverClicked() override;
  void OnNoInteraction(bool is_update) override;
  void PermanentlyBlacklist() override;
  void OnPasswordsRevealed() override;

 private:
  PasswordForm pending_form_;
  std::vector<const PasswordForm*> matches_;
  const std::vector<PasswordForm> federated_matches_;
  const std::vector<PasswordForm> non_federated_matches_;

  // Observer that waits for bubble interaction.
  // The first parameter is true iff the bubble was accepted.
  // The second parameter is the pending form.
  base::RepeatingCallback<void(bool, const PasswordForm&)>
      bubble_interaction_cb_;
};

PasswordDataForUI::PasswordDataForUI(
    PasswordForm pending_form,
    const std::vector<const PasswordForm*>& matches,
    const std::vector<const PasswordForm*>& federated,
    base::RepeatingCallback<void(bool, const PasswordForm&)> bubble_interaction)
    : pending_form_(std::move(pending_form)),
      federated_matches_(DeepCopyVector(federated)),
      non_federated_matches_(DeepCopyVector(matches)),
      bubble_interaction_cb_(std::move(bubble_interaction)) {
  for (const PasswordForm& form : non_federated_matches_)
    matches_.push_back(&form);
}

const GURL& PasswordDataForUI::GetOrigin() const {
  return pending_form_.origin;
}

const std::vector<const PasswordForm*>& PasswordDataForUI::GetBestMatches()
    const {
  return matches_;
}

std::vector<const PasswordForm*> PasswordDataForUI::GetFederatedMatches()
    const {
  std::vector<const PasswordForm*> result(federated_matches_.size());
  std::transform(federated_matches_.begin(), federated_matches_.end(),
                 result.begin(),
                 [](const PasswordForm& form) { return &form; });
  return result;
}

const PasswordForm& PasswordDataForUI::GetPendingCredentials() const {
  return pending_form_;
}

metrics_util::CredentialSourceType PasswordDataForUI::GetCredentialSource()
    const {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* PasswordDataForUI::GetMetricsRecorder() {
  return nullptr;
}

base::span<const InteractionsStats> PasswordDataForUI::GetInteractionsStats()
    const {
  return {};
}

bool PasswordDataForUI::IsBlacklisted() const {
  // 'true' would suppress the bubble.
  return false;
}

void PasswordDataForUI::Save() {
  LogPresavedUpdateUIDismissalReason(metrics_util::CLICKED_SAVE);
  bubble_interaction_cb_.Run(true, pending_form_);
}

void PasswordDataForUI::Update(const PasswordForm&) {
  // The method is obsolete.
  NOTREACHED();
}

void PasswordDataForUI::OnUpdateUsernameFromPrompt(
    const base::string16& new_username) {
  pending_form_.username_value = new_username;
}

void PasswordDataForUI::OnUpdatePasswordFromPrompt(
    const base::string16& new_password) {
  // Ignore. The generated password can be edited in-place.
}

void PasswordDataForUI::OnNopeUpdateClicked() {
  LogPresavedUpdateUIDismissalReason(metrics_util::CLICKED_CANCEL);
  bubble_interaction_cb_.Run(false, pending_form_);
}

void PasswordDataForUI::OnNeverClicked() {
  LogPresavedUpdateUIDismissalReason(metrics_util::CLICKED_NEVER);
  bubble_interaction_cb_.Run(false, pending_form_);
}

void PasswordDataForUI::OnNoInteraction(bool is_update) {
  LogPresavedUpdateUIDismissalReason(metrics_util::NO_DIRECT_INTERACTION);
  bubble_interaction_cb_.Run(false, pending_form_);
}

void PasswordDataForUI::PermanentlyBlacklist() {}

void PasswordDataForUI::OnPasswordsRevealed() {}

// Returns a form from |matches| that causes a name conflict with |generated|.
const PasswordForm* FindUsernameConflict(
    const PasswordForm& generated,
    const std::vector<const PasswordForm*>& matches) {
  for (const auto* form : matches) {
    if (form->username_value == generated.username_value)
      return form;
  }
  return nullptr;
}
}  // namespace

PasswordGenerationManager::PasswordGenerationManager(
    FormSaver* form_saver,
    PasswordManagerClient* client)
    : form_saver_(form_saver),
      client_(client),
      clock_(new base::DefaultClock) {}

PasswordGenerationManager::~PasswordGenerationManager() = default;

std::unique_ptr<PasswordGenerationManager> PasswordGenerationManager::Clone(
    FormSaver* form_saver) const {
  auto clone = std::make_unique<PasswordGenerationManager>(form_saver, client_);
  clone->presaved_ = presaved_;
  return clone;
}

void PasswordGenerationManager::GeneratedPasswordAccepted(
    PasswordForm generated,
    const FormFetcher& fetcher,
    base::WeakPtr<PasswordManagerDriver> driver) {
  if (!base::FeatureList::IsEnabled(features::kGenerationNoOverwrites)) {
    // If the feature not enabled, just proceed with the generation.
    driver->GeneratedPasswordAccepted(generated.password_value);
    return;
  }
  // Clear the username value if there are already saved credentials with
  // the same username in order to prevent overwriting.
  std::vector<const PasswordForm*> matches = fetcher.GetNonFederatedMatches();
  if (FindUsernameConflict(generated, matches)) {
    generated.username_value.clear();
    const PasswordForm* conflict = FindUsernameConflict(generated, matches);
    if (conflict) {
      LogGenerationPresaveConflict(
          GenerationPresaveConflict::kConflictWithEmptyUsername);
      auto bubble_launcher = std::make_unique<PasswordDataForUI>(
          std::move(generated), matches, fetcher.GetFederatedMatches(),
          base::BindRepeating(&PasswordGenerationManager::OnPresaveBubbleResult,
                              weak_factory_.GetWeakPtr(), std::move(driver)));
      client_->PromptUserToSaveOrUpdatePassword(std::move(bubble_launcher),
                                                true);
      return;
    } else {
      LogGenerationPresaveConflict(
          GenerationPresaveConflict::kNoConflictWithEmptyUsername);
    }
  } else {
    LogGenerationPresaveConflict(
        GenerationPresaveConflict::kNoUsernameConflict);
  }
  driver->GeneratedPasswordAccepted(generated.password_value);
}

void PasswordGenerationManager::PresaveGeneratedPassword(
    PasswordForm generated,
    const std::vector<const PasswordForm*>& matches) {
  DCHECK(!generated.password_value.empty());
  // Clear the username value if there are already saved credentials with
  // the same username in order to prevent overwriting.
  if (FindUsernameConflict(generated, matches))
    generated.username_value.clear();
  generated.date_created = clock_->Now();
  if (presaved_) {
    form_saver_->UpdateReplace(generated, {} /* matches */,
                               base::string16() /* old_password */,
                               presaved_.value() /* old_primary_key */);
  } else {
    form_saver_->Save(generated, {} /* matches */,
                      base::string16() /* old_password */);
  }
  presaved_ = std::move(generated);
}

void PasswordGenerationManager::PasswordNoLongerGenerated() {
  DCHECK(presaved_);
  form_saver_->Remove(*presaved_);
  presaved_.reset();
}

void PasswordGenerationManager::CommitGeneratedPassword(
    PasswordForm generated,
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  DCHECK(presaved_);
  generated.preferred = true;
  generated.date_last_used = clock_->Now();
  generated.date_created = clock_->Now();
  form_saver_->UpdateReplace(generated, matches, old_password,
                             presaved_.value() /* old_primary_key */);
}

void PasswordGenerationManager::OnPresaveBubbleResult(
    const base::WeakPtr<PasswordManagerDriver>& driver,
    bool accepted,
    const PasswordForm& pending) {
  weak_factory_.InvalidateWeakPtrs();
  if (accepted) {
    driver->GeneratedPasswordAccepted(pending.password_value);
  }
}

}  // namespace password_manager

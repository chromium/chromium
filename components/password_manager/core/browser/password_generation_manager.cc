// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include <map>
#include <unordered_set>
#include <utility>

#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {
namespace {

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
  const GURL& GetURL() const override;
  const std::vector<const PasswordForm*>& GetBestMatches() const override;
  std::vector<const PasswordForm*> GetFederatedMatches() const override;
  const PasswordForm& GetPendingCredentials() const override;
  metrics_util::CredentialSourceType GetCredentialSource() const override;
  PasswordFormMetricsRecorder* GetMetricsRecorder() override;
  base::span<const InteractionsStats> GetInteractionsStats() const override;
  std::vector<const PasswordForm*> GetInsecureCredentials() const override;
  bool IsBlocklisted() const override;
  bool IsMovableToAccountStore() const override;
  void Save() override;
  void Update(const PasswordForm& credentials_to_update) override;
  void OnUpdateUsernameFromPrompt(const std::u16string& new_username) override;
  void OnUpdatePasswordFromPrompt(const std::u16string& new_password) override;
  void OnNopeUpdateClicked() override;
  void OnNeverClicked() override;
  void OnNoInteraction(bool is_update) override;
  void Blocklist() override;
  void OnPasswordsRevealed() override;
  void MoveCredentialsToAccountStore() override;
  void BlockMovingCredentialsToAccountStore() override;

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

const GURL& PasswordDataForUI::GetURL() const {
  return pending_form_.url;
}

const std::vector<const PasswordForm*>& PasswordDataForUI::GetBestMatches()
    const {
  return matches_;
}

std::vector<const PasswordForm*> PasswordDataForUI::GetFederatedMatches()
    const {
  std::vector<const PasswordForm*> result(federated_matches_.size());
  base::ranges::transform(federated_matches_, result.begin(),
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

std::vector<const PasswordForm*> PasswordDataForUI::GetInsecureCredentials()
    const {
  return {};
}

bool PasswordDataForUI::IsBlocklisted() const {
  // 'true' would suppress the bubble.
  return false;
}

bool PasswordDataForUI::IsMovableToAccountStore() const {
  // This is irrelevant for the generation conflict resolution bubble.
  return false;
}

void PasswordDataForUI::Save() {
  bubble_interaction_cb_.Run(true, pending_form_);
}

void PasswordDataForUI::Update(const PasswordForm&) {
  // The method is obsolete.
  NOTREACHED();
}

void PasswordDataForUI::OnUpdateUsernameFromPrompt(
    const std::u16string& new_username) {
  pending_form_.username_value = new_username;
}

void PasswordDataForUI::OnUpdatePasswordFromPrompt(
    const std::u16string& new_password) {
  // Ignore. The generated password can be edited in-place.
}

void PasswordDataForUI::OnNopeUpdateClicked() {
  bubble_interaction_cb_.Run(false, pending_form_);
}

void PasswordDataForUI::OnNeverClicked() {
  bubble_interaction_cb_.Run(false, pending_form_);
}

void PasswordDataForUI::OnNoInteraction(bool is_update) {
  bubble_interaction_cb_.Run(false, pending_form_);
}

void PasswordDataForUI::Blocklist() {}

void PasswordDataForUI::OnPasswordsRevealed() {}

void PasswordDataForUI::MoveCredentialsToAccountStore() {}

void PasswordDataForUI::BlockMovingCredentialsToAccountStore() {}

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

// For the given |password| string, return a set of characters inside the string
// that satisfies |belongs_to_character_class| boolean predicate.
std::unordered_set<char16_t> FindSetOfCharacterClassInPassword(
    const std::u16string& password,
    const base::FunctionRef<bool(char16_t)>& belongs_to_character_class) {
  std::unordered_set<char16_t> result;
  base::ranges::copy_if(password, std::inserter(result, result.begin()),
                        belongs_to_character_class);
  return result;
}

void EmitUmaForCharacterClassChange(const std::string& uma_character_class,
                                    CharacterClassPresenceChange change) {
  base::UmaHistogramEnumeration(
      "PasswordGeneration.EditsInGeneratedPassword." + uma_character_class,
      change);
}

// For the given 2 strings calculates character class presence change, e.g if
// numerics were deleted('abcde123' => 'abcde') or only some characters
// belonging to the class are changed('abcde123' => 'acbde456'). Also emits UMA
// metrics on the change. Returns true if the character class was added or
// removed.
bool CalculateAndEmitPresenceChangeForCharacterClass(
    const std::u16string& suggested_password,
    const std::u16string& submitted_password,
    const base::FunctionRef<bool(char16_t)>& belongs_to_character_class,
    const std::string& uma_character_class) {
  std::unordered_set<char16_t> suggested_character_set =
      FindSetOfCharacterClassInPassword(suggested_password,
                                        belongs_to_character_class);
  std::unordered_set<char16_t> submitted_character_set =
      FindSetOfCharacterClassInPassword(submitted_password,
                                        belongs_to_character_class);

  if (suggested_character_set == submitted_character_set) {
    EmitUmaForCharacterClassChange(uma_character_class,
                                   CharacterClassPresenceChange::kNoChange);
    return false;
  }

  if (suggested_character_set.empty() != submitted_character_set.empty()) {
    EmitUmaForCharacterClassChange(uma_character_class,
                                   submitted_character_set.empty()
                                       ? CharacterClassPresenceChange::kDeleted
                                       : CharacterClassPresenceChange::kAdded);
    return true;
  }

  EmitUmaForCharacterClassChange(
      uma_character_class,
      CharacterClassPresenceChange::kSpecificCharactersChanged);
  return false;
}

// Emits UMA metrics on changes (character classes, length and together as a
// mask) between generated and submitted passwords.
void SendUmaHistogramsOnGeneratedPasswordAttributeChanges(
    const std::u16string& generated_password,
    const std::u16string& submitted_password) {
  bool letters_presence_changed =
      CalculateAndEmitPresenceChangeForCharacterClass(
          generated_password, submitted_password,
          password_manager_util::IsLetter, "Letters");
  // Also emit dedicated metrics for upper- and lowercase letters specifically
  // just in case there are websites that treat them differently (e.g. allow
  // only lowercase letters).
  CalculateAndEmitPresenceChangeForCharacterClass(
      generated_password, submitted_password,
      password_manager_util::IsUppercaseLetter, "Uppercase");
  CalculateAndEmitPresenceChangeForCharacterClass(
      generated_password, submitted_password,
      password_manager_util::IsLowercaseLetter, "Lowercase");
  bool numerics_presence_changed =
      CalculateAndEmitPresenceChangeForCharacterClass(
          generated_password, submitted_password,
          password_manager_util::IsNumeric, "Numerics");
  bool symbols_presence_changed =
      CalculateAndEmitPresenceChangeForCharacterClass(
          generated_password, submitted_password,
          password_manager_util::IsSpecialSymbol, "Symbols");

  bool length_changed = generated_password.size() != submitted_password.size();

  if (length_changed && !letters_presence_changed &&
      !numerics_presence_changed && !symbols_presence_changed) {
    // Only length changed and no character class presence is changed. If
    // character class presence is changed, length increase/decrease will
    // be a side effect. This metric will calculate whether our password is too
    // long or too short, so that it would be possible to adapt length if
    // necessary.
    base::UmaHistogramBoolean(
        "PasswordGeneration.EditsInGeneratedPassword.AlteredLengthIncreased",
        generated_password.size() < submitted_password.size());
  }

  // Make sure the order of attributes coincides with the order of attributes in
  // the metric.
  uint8_t attributes_mask =
      (length_changed << 0) | (letters_presence_changed << 1) |
      (numerics_presence_changed << 2) | (symbols_presence_changed << 3);
  if (attributes_mask != 0) {
    base::UmaHistogramExactLinear(
        "PasswordGeneration.EditsInGeneratedPassword.AttributesMask",
        attributes_mask, 16 /*exclusive_max*/);
  }
}

}  // namespace

PasswordGenerationManager::PasswordGenerationManager(
    PasswordManagerClient* client)
    : client_(client) {}

PasswordGenerationManager::~PasswordGenerationManager() = default;

std::unique_ptr<PasswordGenerationManager> PasswordGenerationManager::Clone()
    const {
  auto clone = std::make_unique<PasswordGenerationManager>(client_);
  clone->presaved_ = presaved_;
  return clone;
}

void PasswordGenerationManager::GeneratedPasswordAccepted(
    PasswordForm generated,
    const std::vector<const PasswordForm*>& non_federated_matches,
    const std::vector<const PasswordForm*>& federated_matches,
    base::WeakPtr<PasswordManagerDriver> driver) {
  // Clear the username value if there are already saved credentials with
  // the same username in order to prevent overwriting.
  if (FindUsernameConflict(generated, non_federated_matches)) {
    generated.username_value.clear();
    const PasswordForm* conflict =
        FindUsernameConflict(generated, non_federated_matches);
    if (conflict) {
      auto bubble_launcher = std::make_unique<PasswordDataForUI>(
          std::move(generated), non_federated_matches, federated_matches,
          base::BindRepeating(&PasswordGenerationManager::OnPresaveBubbleResult,
                              weak_factory_.GetWeakPtr(), std::move(driver)));
      client_->PromptUserToSaveOrUpdatePassword(std::move(bubble_launcher),
                                                true);
      return;
    }
  }
  driver->GeneratedPasswordAccepted(generated.password_value);
}

void PasswordGenerationManager::PresaveGeneratedPassword(
    PasswordForm generated,
    const std::vector<const PasswordForm*>& matches,
    FormSaver* form_saver) {
  CHECK(!generated.password_value.empty());
  // Clear the username value if there are already saved credentials with
  // the same username in order to prevent overwriting.
  if (FindUsernameConflict(generated, matches))
    generated.username_value.clear();
  generated.date_created = base::Time::Now();
  if (presaved_) {
    form_saver->UpdateReplace(generated, {} /* matches */,
                              std::u16string() /* old_password */,
                              presaved_.value() /* old_primary_key */);
  } else {
    form_saver->Save(generated, {} /* matches */,
                     std::u16string() /* old_password */);
    initial_generated_password_ = generated.password_value;
  }
  presaved_ = std::move(generated);
}

void PasswordGenerationManager::PasswordNoLongerGenerated(
    FormSaver* form_saver) {
  DCHECK(presaved_);
  form_saver->Remove(*presaved_);
  presaved_.reset();
  initial_generated_password_.clear();
}

void PasswordGenerationManager::CommitGeneratedPassword(
    PasswordForm generated,
    const std::vector<const PasswordForm*>& matches,
    const std::u16string& old_password,
    FormSaver* form_saver) {
  DCHECK(presaved_);
  generated.date_last_used = base::Time::Now();
  generated.date_created = base::Time::Now();
  if (initial_generated_password_ != generated.password_value) {
    // If the generated password was edited, send UMA metrics on what kind of
    // changes were there.
    SendUmaHistogramsOnGeneratedPasswordAttributeChanges(
        initial_generated_password_, generated.password_value);
  }
  form_saver->UpdateReplace(generated, matches, old_password,
                            presaved_.value() /* old_primary_key */);
  presaved_ = std::move(generated);
}

void PasswordGenerationManager::OnPresaveBubbleResult(
    const base::WeakPtr<PasswordManagerDriver>& driver,
    bool accepted,
    const PasswordForm& pending) {
  weak_factory_.InvalidateWeakPtrs();
  // See https://crbug.com/1210341 for when `driver` might be null due to a
  // compromised renderer.
  if (!driver) {
    return;
  }

  if (accepted) {
    driver->GeneratedPasswordAccepted(pending.password_value);
  } else {
    driver->ClearPreviewedForm();
  }
}

}  // namespace password_manager

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {

class UserData;
struct LoginChoice;
struct CollectUserDataOptions;

// Manages a map of |ValueProto| instances and notifies observers of changes.
//
// - It is safe to add/remove observers at any time.
// - Provides a == comparison operator for |ValueProto|.
// - Provides a << output operator for |ValueProto| for debugging.
class UserModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    ~Observer() override;

    virtual void OnValueChanged(const std::string& identifier,
                                const ValueProto& new_value) = 0;
  };

  UserModel();

  UserModel(const UserModel&) = delete;
  UserModel& operator=(const UserModel&) = delete;

  virtual ~UserModel();

  base::WeakPtr<UserModel> GetWeakPtr();

  // Writes |value| to |identifier|, potentially overwriting the previously
  // stored value. If the new value is different or |force_notification| is
  // true, a change notification will be fired.
  void SetValue(const std::string& identifier,
                const ValueProto& value,
                bool force_notification = false);

  // Returns the value for |identifier| or nullopt if there is no such value.
  // - Placeholders in |identifier| of the form ${key} are automatically
  // replaced (see |AddIdentifierPlaceholders|).
  // - Also supports the array operator to retrieve
  // a specific element of a list, e.g., "identifier[0]" to get the first item.
  absl::optional<ValueProto> GetValue(const std::string& identifier) const;

  // Returns the value for |reference| or nullopt if there is no such value.
  absl::optional<ValueProto> GetValue(
      const ValueReferenceProto& reference) const;

  // Returns all specified values in a new std::vector. Returns nullopt if any
  // of the requested values was not found.
  template <class T>
  absl::optional<std::vector<ValueProto>> GetValues(
      const T& value_references) const {
    std::vector<ValueProto> values;
    for (const auto& reference : value_references) {
      auto value = GetValue(reference);
      if (!value.has_value()) {
        return absl::nullopt;
      }
      values.emplace_back(*value);
    }
    return values;
  }

  // Replaces the set of available autofill credit cards.
  void SetAutofillCreditCards(
      std::unique_ptr<std::vector<std::unique_ptr<autofill::CreditCard>>>
          credit_cards);

  // Sets the selected credit card. A nullptr |card| will clear the selected
  // card. This also sets it to |user_data|.
  // TODO(b/187286050) complete the migration to UserModel and remove UserData.
  virtual void SetSelectedCreditCard(std::unique_ptr<autofill::CreditCard> card,
                                     UserData* user_data);

  // Sets the selected login choice. A nullptr |login_choice| will clear the
  // selected login choice. This sets it to |user_data|.
  // TODO(b/187286050) complete the migration to UserModel and remove UserData.
  void SetSelectedLoginChoice(std::unique_ptr<LoginChoice> login_choice,
                              UserData* user_data);

  // Sets the selected login choice. If the identifier can not be found the
  // selected login choice will be cleared. This sets it to |user_data|.
  // TODO(b/187286050) complete the migration to UserModel and remove UserData.
  void SetSelectedLoginChoiceByIdentifier(
      const std::string& identifier,
      const CollectUserDataOptions& collect_user_data_options,
      UserData* user_data);

  // Replaces the set of available autofill profiles.
  void SetAutofillProfiles(
      std::unique_ptr<std::vector<std::unique_ptr<autofill::AutofillProfile>>>
          profiles);

  // Sets the selected autofill profile for |profile_name|. A nullptr |profile|
  // will clear the entry. The profile is also set in |user_data|.
  // TODO(b/187286050) complete the migration to UserModel and remove UserData.
  virtual void SetSelectedAutofillProfile(
      const std::string& profile_name,
      std::unique_ptr<autofill::AutofillProfile> profile,
      UserData* user_data);

  void SetCurrentURL(GURL current_url);

  // Returns the credit card specified by |proto| or nullptr if there is no such
  // card.
  const autofill::CreditCard* GetCreditCard(
      const AutofillCreditCardProto& proto) const;

  // Returns the selected credit card or nullptr if no card has been selected.
  const autofill::CreditCard* GetSelectedCreditCard() const;

  // Returns the profile specified by |proto| or nullptr if there is no such
  // profile.
  const autofill::AutofillProfile* GetProfile(
      const AutofillProfileProto& proto) const;

  // Returns the selected profile for the specified |profile_name| or nullptr if
  // there is no such profile.
  const autofill::AutofillProfile* GetSelectedAutofillProfile(
      const std::string& profile_name) const;

  GURL GetCurrentURL() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Merges *this with |another| such that the result is the union of both.
  // In case of ambiguity, |another| takes precedence. Empty values in
  // |another| do not overwrite non-empty values in *this.
  // If |force_notifications| is true, a value-changed notification will be
  // fired for every value in |another|, even if the value has not changed.
  void MergeWithProto(const ModelProto& another, bool force_notifications);

  // Updates the current values of all identifiers contained in |model_proto|.
  void UpdateProto(ModelProto* model_proto) const;

 private:
  friend class UserModelTest;

  base::flat_map<std::string, ValueProto> values_;
  // Guid to credit card map.
  base::flat_map<std::string, std::unique_ptr<autofill::CreditCard>>
      credit_cards_;
  // The selected credit card.
  std::unique_ptr<autofill::CreditCard> selected_card_;
  // Guid to profile map.
  base::flat_map<std::string, std::unique_ptr<autofill::AutofillProfile>>
      profiles_;
  // Profile name to profile map.
  base::flat_map<std::string, std::unique_ptr<autofill::AutofillProfile>>
      selected_profiles_;

  GURL current_url_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<UserModel> weak_ptr_factory_{this};
};

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_

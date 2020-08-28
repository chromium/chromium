// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "url/gurl.h"

namespace autofill_assistant {

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
  ~UserModel();

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
  base::Optional<ValueProto> GetValue(const std::string& identifier) const;

  // Returns the value for |reference| or nullopt if there is no such value.
  base::Optional<ValueProto> GetValue(
      const ValueReferenceProto& reference) const;

  // Returns all specified values in a new std::vector. Returns nullopt if any
  // of the requested values was not found.
  template <class T>
  base::Optional<std::vector<ValueProto>> GetValues(
      const T& value_references) const {
    std::vector<ValueProto> values;
    for (const auto& reference : value_references) {
      auto value = GetValue(reference);
      if (!value.has_value()) {
        return base::nullopt;
      }
      values.emplace_back(*value);
    }
    return values;
  }

  // Replaces the set of available autofill credit cards.
  void SetAutofillCreditCards(
      std::unique_ptr<std::vector<std::unique_ptr<autofill::CreditCard>>>
          credit_cards);

  // Replaces the set of available autofill profiles.
  void SetAutofillProfiles(
      std::unique_ptr<std::vector<std::unique_ptr<autofill::AutofillProfile>>>
          profiles);

  void SetCurrentURL(GURL current_url);

  // Returns the credit card with |guid| or nullptr if there is no such card.
  const autofill::CreditCard* GetCreditCard(const std::string& guid) const;

  // Returns the profile with |guid| or nullptr if there is no such profile.
  const autofill::AutofillProfile* GetProfile(const std::string& guid) const;

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

  std::map<std::string, ValueProto> values_;
  std::map<std::string, std::unique_ptr<autofill::CreditCard>> credit_cards_;
  std::map<std::string, std::unique_ptr<autofill::AutofillProfile>> profiles_;
  GURL current_url_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<UserModel> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(UserModel);
};

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

#include <string>
#include <vector>

#include "base/logging.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"

namespace autofill {

class CreditCard;

// For classic Autofill form fields, the KeyType is AutofillKey.
// Autofill++ types such as AutofillProfile and CreditCard simply use a string.
template <typename KeyType>
class GenericAutofillChange {
 public:
  enum Type { ADD, UPDATE, REMOVE, EXPIRE };

  virtual ~GenericAutofillChange() {}

  Type type() const { return type_; }
  const KeyType& key() const { return key_; }

 protected:
  GenericAutofillChange(Type type, const KeyType& key)
      : type_(type),
        key_(key) {}
 private:
  Type type_;
  KeyType key_;
};

class AutofillChange : public GenericAutofillChange<AutofillKey> {
 public:
  AutofillChange(Type type, const AutofillKey& key);
  ~AutofillChange() override;
  bool operator==(const AutofillChange& change) const {
    return type() == change.type() && key() == change.key();
  }
};

typedef std::vector<AutofillChange> AutofillChangeList;

// Change notification details for Autofill profile or credit card changes.
template <typename DataType>
class AutofillDataModelChange : public GenericAutofillChange<std::string> {
 public:
  // The |type| input specifies the change type.  The |key| input is the key
  // that identifies the |data_model|; it is the GUID of the entry for local
  // data and server_id of the entry for server data from GPay.
  AutofillDataModelChange(Type type,
                          const std::string& key,
                          const DataType* data_model)
      : GenericAutofillChange<std::string>(type, key), data_model_(data_model) {
    DCHECK(data_model &&
           (data_model->guid() == key || data_model->server_id() == key));
  }

  ~AutofillDataModelChange() override {}

  const DataType* data_model() const { return data_model_; }

  bool operator==(const AutofillDataModelChange<DataType>& change) const {
    return type() == change.type() && key() == change.key() &&
           (type() == REMOVE || *data_model() == *change.data_model());
  }

 private:
  // Weak reference, can be NULL.
  const DataType* data_model_;
};

typedef AutofillDataModelChange<AutofillProfile> AutofillProfileChange;
typedef AutofillDataModelChange<CreditCard> CreditCardChange;

class AutofillProfileDeepChange : public AutofillProfileChange {
 public:
  AutofillProfileDeepChange(Type type, const AutofillProfile& profile)
      : AutofillProfileChange(type, profile.guid(), &profile),
        profile_(profile) {}

  ~AutofillProfileDeepChange() override {}

  const AutofillProfile* profile() const { return &profile_; }
  bool is_ongoing_on_background() const { return is_ongoing_on_background_; }
  void set_is_ongoing_on_background() const {
    is_ongoing_on_background_ = true;
  }

  void validation_effort_made() const { validation_effort_made_ = true; }
  bool has_validation_effort_made() const { return validation_effort_made_; }

  void set_enforced() { enforced_ = true; }
  bool enforced() const { return enforced_; }

 private:
  AutofillProfile profile_;
  // Is true when the change is taking place on the database side on the
  // background.
  mutable bool is_ongoing_on_background_ = false;
  // Is true when the |profile_| has gone through the validation process.
  // Note: This could be different from the
  // profile_.is_client_validity_states_updated. |validation_effort_made_| shows
  // that the effort has been made, but not necessarily successful, and profile
  // validity may or may not be updated.
  mutable bool validation_effort_made_ = false;

  // Is true when the change should happen regardless of an existing or equal
  // profile.
  mutable bool enforced_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

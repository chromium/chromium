// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"

namespace autofill {

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

using AutofillChangeList = std::vector<AutofillChange>;

template <
    typename DataType,
    std::enable_if_t<std::disjunction_v<std::is_same<DataType, AutofillProfile>,
                                        std::is_same<DataType, CreditCard>,
                                        std::is_same<DataType, Iban>>,
                     bool> = true>
bool DataModelEntryMatchesKey(const DataType& model, const std::string& key) {
  if constexpr (std::is_same_v<DataType, Iban>) {
    // `Iban` does not have `server_id`.
    return model.guid() == key || model.instrument_id() == key;
  } else {
    return model.guid() == key || model.server_id() == key;
  }
}

template <typename DataType,
          std::enable_if_t<std::is_same_v<DataType, ServerCvc>, bool> = true>
bool DataModelEntryMatchesKey(const DataType& model, const std::string& key) {
  return base::NumberToString(model.instrument_id) == key;
}

// Change notification details for Autofill related changes.
// TODO(crbug/1476099): Update the name for `AutofillDataModelChange` as it now
// captures non data model changes.
template <typename DataType>
class AutofillDataModelChange : public GenericAutofillChange<std::string> {
 public:
  // The `type` input specifies the change type.  The `key` input is the key
  // that identifies the `data_model`; it is the GUID of the entry for local
  // data and server_id of the entry for server data from GPay.
  AutofillDataModelChange(Type type,
                          const std::string& key,
                          DataType data_model)
      : GenericAutofillChange<std::string>(type, key),
        data_model_(std::move(data_model)) {
    // For `REMOVE`, the `data_model_` isn't needed (because for deletions,
    // Sync doesn't send the data to the server).
    CHECK(type == REMOVE || DataModelEntryMatchesKey(data_model_, key));
  }

  ~AutofillDataModelChange() override = default;

  const DataType& data_model() const { return data_model_; }

  bool operator==(const AutofillDataModelChange<DataType>& change) const {
    return type() == change.type() && key() == change.key() &&
           (type() == REMOVE || data_model() == change.data_model());
  }

 private:
  DataType data_model_;
};

using AutofillProfileChange = AutofillDataModelChange<AutofillProfile>;
using CreditCardChange = AutofillDataModelChange<CreditCard>;
using IbanChange = AutofillDataModelChange<Iban>;
using ServerCvcChange = AutofillDataModelChange<ServerCvc>;

class AutofillProfileDeepChange : public AutofillProfileChange {
 public:
  AutofillProfileDeepChange(Type type, const AutofillProfile& profile)
      : AutofillProfileChange(type, profile.guid(), profile) {}

  const AutofillProfile& profile() const {
    return static_cast<const AutofillProfile&>(data_model());
  }
  bool is_ongoing_on_background() const { return is_ongoing_on_background_; }
  void set_is_ongoing_on_background() const {
    is_ongoing_on_background_ = true;
  }

  void set_enforced() { enforced_ = true; }
  bool enforced() const { return enforced_; }

 private:
  // Is true when the change is taking place on the database side on the
  // background.
  mutable bool is_ongoing_on_background_ = false;

  // Is true when the change should happen regardless of an existing or equal
  // profile.
  mutable bool enforced_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

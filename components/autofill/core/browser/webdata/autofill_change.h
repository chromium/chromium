// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

#include <string>
#include <type_traits>
#include <vector>

#include "base/check.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/webdata/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"

namespace autofill {

class AutocompleteChange {
 public:
  enum Type { ADD, UPDATE, REMOVE, EXPIRE };

  AutocompleteChange(Type type, const AutocompleteKey& key);
  ~AutocompleteChange();

  Type type() const { return type_; }
  const AutocompleteKey& key() const { return key_; }

  bool operator==(const AutocompleteChange& change) const {
    return type() == change.type() && key() == change.key();
  }

 private:
  Type type_;
  AutocompleteKey key_;
};

using AutocompleteChangeList = std::vector<AutocompleteChange>;

// Change notification details for Autofill related changes.
// TODO(crbug/1476099): Update the name for `AutofillDataModelChange` as it now
// captures non data model changes.
template <typename DataType>
class AutofillDataModelChange {
 public:
  static_assert(std::disjunction_v<std::is_base_of<AutofillDataModel, DataType>,
                                   std::is_same<DataType, ServerCvc>>);

  enum Type { ADD, UPDATE, REMOVE };

  // The `type` input specifies the change type.  The `key` input is the key
  // that identifies the `data_model`; it is the GUID of the entry for local
  // data and server_id of the entry for server data from GPay.
  AutofillDataModelChange(Type type,
                          const std::string& key,
                          DataType data_model)
      : type_(type), key_(key), data_model_(std::move(data_model)) {
    // Verify that the `key_` corresponds to the `data_model_`. For REMOVE
    // changes, the `data_model` is optional and hence the check skipped.
    if (type_ == REMOVE) {
      return;
    }
    if constexpr (std::is_same_v<DataType, Iban>) {
      CHECK(data_model_.guid() == key_ || data_model_.instrument_id() == key_);
    } else if constexpr (std::is_same_v<DataType, ServerCvc>) {
      CHECK(base::NumberToString(data_model_.instrument_id) == key_);
    } else {
      // TODO(crbug.com/1475085): Use `instrument_id()` for credit cards and
      // merge the `Iban` and `CreditCard` cases.
      // TODO(crbug.com/1457187): Once server addresses are removed,
      // `server_id()` becomes irrelevant for `AutofillProfile`.
      CHECK(data_model_.guid() == key_ || data_model_.server_id() == key_);
    }
  }

  ~AutofillDataModelChange() = default;

  Type type() const { return type_; }
  const std::string& key() const { return key_; }
  const DataType& data_model() const { return data_model_; }

  bool operator==(const AutofillDataModelChange<DataType>& change) const {
    return type() == change.type() && key() == change.key() &&
           (type() == REMOVE || data_model() == change.data_model());
  }

 private:
  Type type_;
  std::string key_;
  DataType data_model_;
};

using AutofillProfileChange = AutofillDataModelChange<AutofillProfile>;
using CreditCardChange = AutofillDataModelChange<CreditCard>;
using IbanChange = AutofillDataModelChange<Iban>;
using ServerCvcChange = AutofillDataModelChange<ServerCvc>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

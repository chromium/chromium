// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

#include <concepts>
#include <string>
#include <variant>
#include <vector>

#include "base/check.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"

namespace autofill {

class AutocompleteChange {
 public:
  enum Type { ADD, UPDATE, REMOVE, EXPIRE };

  AutocompleteChange(Type type, const AutocompleteKey& key);
  ~AutocompleteChange();

  Type type() const { return type_; }
  const AutocompleteKey& key() const { return key_; }

  bool operator==(const AutocompleteChange&) const = default;

 private:
  Type type_;
  AutocompleteKey key_;
};

using AutocompleteChangeList = std::vector<AutocompleteChange>;

// Change notification details for Autofill related changes.
// TODO(crbug.com/40928146): Update the name for `AutofillDataModelChange` as it
// now captures non data model changes.
template <typename DataType, typename KeyType>
  requires std::same_as<DataType, AutofillProfile> ||
           std::same_as<DataType, std::optional<EntityInstance>> ||
           std::same_as<DataType, CreditCard> || std::same_as<DataType, Iban> ||
           std::same_as<DataType, ServerCvc>
class AutofillDataModelChange {
 public:
  // The difference between `REMOVE` and `HIDE_IN_AUTOFILL` is that the latter
  // does not actually remove the profile from the server, but instead marks
  // it as uninteresting to Chrome. This profile may become visible again if
  // it is updated in a different product.
  enum Type { ADD, UPDATE, REMOVE, HIDE_IN_AUTOFILL };

  // The `type` input specifies the change type.  The `key` input is the key
  // that identifies the `data_model`; it is the GUID of the entry for local
  // data and server_id of the entry for server data from GPay.
  AutofillDataModelChange(Type type, KeyType key, DataType data_model)
      : type_(type), key_(std::move(key)), data_model_(std::move(data_model)) {
    // Verify that the `key_` corresponds to the `data_model_`. For REMOVE
    // changes, the `data_model` is optional and hence the check skipped.
    if (type_ == REMOVE) {
      return;
    }
    if constexpr (std::same_as<DataType, Iban>) {
      if (data_model_.record_type() == Iban::RecordType::kLocalIban) {
        CHECK(std::holds_alternative<std::string>(key_) &&
              std::get<std::string>(key_) == data_model_.guid());
      } else {
        CHECK(data_model_.record_type() == Iban::RecordType::kServerIban);
        CHECK(std::holds_alternative<int64_t>(key_) &&
              std::get<int64_t>(key_) == data_model_.instrument_id());
      }
    } else if constexpr (std::same_as<DataType, ServerCvc>) {
      CHECK(data_model_.instrument_id == key_);
    } else if constexpr (std::same_as<DataType, CreditCard>) {
      // TODO(crbug.com/40927747): Use `instrument_id()` for credit cards and
      // merge the `Iban` and `CreditCard` cases.
      CHECK(data_model_.guid() == key_ || data_model_.server_id() == key_);
    } else if constexpr (std::same_as<DataType,
                                      std::optional<EntityInstance>>) {
      CHECK(data_model_ && data_model_->guid() == key_);
    } else {
      CHECK(data_model_.guid() == key_);
    }
  }

  ~AutofillDataModelChange() = default;

  Type type() const { return type_; }
  const KeyType& key() const { return key_; }
  const DataType& data_model() const { return data_model_; }

  bool operator==(
      const AutofillDataModelChange<DataType, KeyType>& change) const {
    return type() == change.type() && key() == change.key() &&
           (type() == REMOVE || data_model() == change.data_model());
  }

 private:
  Type type_;
  KeyType key_;
  DataType data_model_;
};

// Identified by `AutofillProfile::guid()`.
using AutofillProfileChange =
    AutofillDataModelChange<AutofillProfile, std::string>;

// Identified by `EntityInstance::guid()`. The EntityInstance is present for
// `ADD` and `UPDATE` operations but absent for `REMOVE` operations.
using EntityInstanceChange =
    AutofillDataModelChange<std::optional<EntityInstance>, base::Uuid>;

// Identified by `CreditCard::guid()` for local cards and
// `CreditCard::server_id()` for server cards.
// TODO(crbug.com/40927747): For server cards, an instrument id should be used.
using CreditCardChange = AutofillDataModelChange<CreditCard, std::string>;

// Identified by `Iban::guid()` for local IBANs and `Iban::instrument_id()` for
// server IBANs.
using IbanChange =
    AutofillDataModelChange<Iban, std::variant<std::string, int64_t>>;

// Identified by `ServerCvc::instrument_id`.
using ServerCvcChange = AutofillDataModelChange<ServerCvc, int64_t>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_CHANGE_H__

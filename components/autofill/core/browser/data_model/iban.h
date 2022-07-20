// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_

#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"

namespace autofill {

struct AutofillMetadata;

// A form group that stores IBAN information.
class Iban : public AutofillDataModel {
 public:
  enum RecordType {
    // An IBAN stored and editable locally.
    // Note: We only have local IBAN for now.
    LOCAL_IBAN,
    // An IBAN synced down from the server. These are read-only locally.
    // Note: Server IBAN is not supported for now.
    SERVER_IBAN,
  };

  explicit Iban(const std::string& guid);

  // For use in STL containers.
  Iban();
  Iban(const Iban&);
  ~Iban() override;

  void operator=(const Iban& iban);

  // AutofillDataModel:
  AutofillMetadata GetMetadata() const override;
  bool SetMetadata(const AutofillMetadata metadata) override;

  // Whether the IBAN is deletable. Always returns false for now as IBAN
  // never expires.
  bool IsDeletable() const override;

  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(
      ServerFieldType type,
      const std::u16string& value,
      structured_address::VerificationStatus status) override;
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  // How this IBAN is stored.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType type) { record_type_ = type; }

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Comparison for Sync. Returns 0 if |iban| is the same as this, or < 0,
  // or > 0 if it is different. The implied ordering can be used for culling
  // duplicates. The ordering is based on the collation order of the textual
  // contents of the fields.
  // GUIDs, origins, and server id are not compared, only the values of
  // the IBANs themselves.
  int Compare(const Iban& iban) const;

  // Equality operators compare GUIDs, origins, |record_type_|, |value_|,
  // |nickname_| and the |server_id_|.
  bool operator==(const Iban& iban) const;
  virtual bool operator!=(const Iban& iban) const;

  // Returns the ID assigned by the server. |server_id_| is empty if it's a
  // local IBAN.
  const std::string& server_id() const { return server_id_; }

  // Returns the value (the actual bank account number) of IBAN.
  const std::u16string& value() const { return value_; }
  void set_value(const std::u16string& value) { value_ = value; }

  // Returns a constant reference to the |nickname_| field.
  const std::u16string& nickname() const { return nickname_; }
  // Set the |nickname_| with the processed input (replace all tabs and newlines
  // with whitespaces, condense multiple whitespaces into a single one, and
  // trim leading/trailing whitespaces).
  void set_nickname(const std::u16string& nickname);

  // Returns a constant reference to the |iban_account_holder_name_| field.
  const std::u16string& iban_account_holder_name() const {
    return iban_account_holder_name_;
  }
  void set_iban_account_holder_name(
      const std::u16string& iban_account_holder_name) {
    iban_account_holder_name_ = iban_account_holder_name;
  }

 private:
  // This is the ID assigned by the server to uniquely identify this card.
  // Note: server_id is empty for now as only local IBAN is supported.
  std::string server_id_;

  // Type of how IBAN is stored, either local or server.
  // Note: IBAN will only be stored locally for now.
  RecordType record_type_;

  // The IBAN's value, i.e., the actual bank account number.
  std::u16string value_;

  // The nickname of the IBAN. May be empty.
  std::u16string nickname_;

  // Account holder name of the IBAN. May be empty.
  std::u16string iban_account_holder_name_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_

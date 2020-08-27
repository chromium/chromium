// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_METHOD_MANIFEST_TABLE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_METHOD_MANIFEST_TABLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace payments {

struct SecurePaymentConfirmationInstrument;

// This class manages Web Payment tables in SQLite database. It expects the
// following schema.
//
// payment_method_manifest This table stores WebAppManifestSection.id of the
//                         supported web app in this payment method manifest.
//                         Note that a payment method manifest might contain
//                         multiple supported web apps ids.
//
//   expire_date           The expire date in seconds from 1601-01-01 00:00:00
//                         UTC.
//   method_name           The method name.
//   web_app_id            The supported web app id.
//                         (WebAppManifestSection.id).
//
// secure_payment_confirmation_instrument
//                         This table stores instrument information for secure
//                         payment confirmation method.
//
//   credential_id         The WebAuthn credential identifier blob. Primary key.
//   relying_party_id      The relying party identifier string.
//   label                 The instrument human-readable label string.
//   icon                  The serialized SkBitmap blob.
class PaymentMethodManifestTable : public WebDatabaseTable {
 public:
  PaymentMethodManifestTable();
  ~PaymentMethodManifestTable() override;

  PaymentMethodManifestTable(const PaymentMethodManifestTable& other) = delete;
  PaymentMethodManifestTable& operator=(
      const PaymentMethodManifestTable& other) = delete;

  // Retrieves the PaymentMethodManifestTable* owned by `db`.
  static PaymentMethodManifestTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool IsSyncable() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Remove expired data.
  void RemoveExpiredData();

  // Adds `payment_method`'s manifest. `web_app_ids` contains supported web apps
  // ids.
  bool AddManifest(const std::string& payment_method,
                   const std::vector<std::string>& web_app_ids);

  // Gets manifest for `payment_method`. Return empty vector if no manifest
  // exists for this method.
  std::vector<std::string> GetManifest(const std::string& payment_method);

  // Adds a secure payment confirmation `instrument`. All existing data for the
  // instrument's (relying_party_id, credential_id) tuple is erased before the
  // new data is added.
  //
  // Each field in the `instrument` should be non-empty and `relying_party_id`
  // field should be a valid domain string. See:
  // https://url.spec.whatwg.org/#valid-domain-string
  //
  // Returns false for invalid data, e.g., credential reuse between relying
  // parties, or on failure.
  bool AddSecurePaymentConfirmationInstrument(
      const SecurePaymentConfirmationInstrument& instrument);

  // Gets the list of secure payment confirmation instruments for the given list
  // of `credential_ids`.
  //
  // Returns an empty vector when no data is found or when a read error occurs.
  // Does not return invalid instruments.
  //
  // Please use `std::move()` for `credential_ids` parameter to avoid extra
  // copies.
  std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>>
  GetSecurePaymentConfirmationInstruments(
      std::vector<std::vector<uint8_t>> credential_ids);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_METHOD_MANIFEST_TABLE_H_
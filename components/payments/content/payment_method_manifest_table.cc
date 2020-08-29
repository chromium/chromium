// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_method_manifest_table.h"

#include <time.h>

#include "base/notreached.h"
#include "base/time/time.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace payments {
namespace {

// Data valid duration in seconds.
const time_t PAYMENT_METHOD_MANIFEST_VALID_TIME_IN_SECONDS = 90 * 24 * 60 * 60;

WebDatabaseTable::TypeKey GetPaymentMethodManifestKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

PaymentMethodManifestTable::PaymentMethodManifestTable() = default;

PaymentMethodManifestTable::~PaymentMethodManifestTable() = default;

PaymentMethodManifestTable* PaymentMethodManifestTable::FromWebDatabase(
    WebDatabase* db) {
  return static_cast<PaymentMethodManifestTable*>(
      db->GetTable(GetPaymentMethodManifestKey()));
}

WebDatabaseTable::TypeKey PaymentMethodManifestTable::GetTypeKey() const {
  return GetPaymentMethodManifestKey();
}

bool PaymentMethodManifestTable::CreateTablesIfNecessary() {
  if (!db_->Execute("CREATE TABLE IF NOT EXISTS payment_method_manifest ( "
                    "expire_date INTEGER NOT NULL DEFAULT 0, "
                    "method_name VARCHAR, "
                    "web_app_id VARCHAR)")) {
    NOTREACHED();
    return false;
  }

  // The `credential_id` column is 20 bytes for UbiKey on Linux, but the size
  // can vary for different authenticators. The relatively small sizes make it
  // OK to make `credential_id` the primary key.
  if (!db_->Execute(
          "CREATE TABLE IF NOT EXISTS secure_payment_confirmation_instrument ( "
          "credential_id BLOB NOT NULL PRIMARY KEY, "
          "relying_party_id VARCHAR NOT NULL, "
          "label VARCHAR NOT NULL, "
          "icon BLOB NOT NULL)")) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool PaymentMethodManifestTable::IsSyncable() {
  return false;
}

bool PaymentMethodManifestTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  return true;
}

void PaymentMethodManifestTable::RemoveExpiredData() {
  const time_t now_date_in_seconds = base::Time::NowFromSystemTime().ToTimeT();
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM payment_method_manifest WHERE expire_date < ?"));
  s.BindInt64(0, now_date_in_seconds);
  s.Run();
}

bool PaymentMethodManifestTable::AddManifest(
    const std::string& payment_method,
    const std::vector<std::string>& web_app_ids) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  sql::Statement s1(db_->GetUniqueStatement(
      "DELETE FROM payment_method_manifest WHERE method_name=?"));
  s1.BindString(0, payment_method);
  if (!s1.Run())
    return false;

  sql::Statement s2(
      db_->GetUniqueStatement("INSERT INTO payment_method_manifest "
                              "(expire_date, method_name, web_app_id) "
                              "VALUES (?, ?, ?)"));
  const time_t expire_date_in_seconds =
      base::Time::NowFromSystemTime().ToTimeT() +
      PAYMENT_METHOD_MANIFEST_VALID_TIME_IN_SECONDS;
  for (const auto& id : web_app_ids) {
    int index = 0;
    s2.BindInt64(index++, expire_date_in_seconds);
    s2.BindString(index++, payment_method);
    s2.BindString(index, id);
    if (!s2.Run())
      return false;
    s2.Reset(true);
  }

  if (!transaction.Commit())
    return false;

  return true;
}

std::vector<std::string> PaymentMethodManifestTable::GetManifest(
    const std::string& payment_method) {
  std::vector<std::string> web_app_ids;
  sql::Statement s(
      db_->GetUniqueStatement("SELECT web_app_id "
                              "FROM payment_method_manifest "
                              "WHERE method_name=?"));
  s.BindString(0, payment_method);

  while (s.Step()) {
    web_app_ids.emplace_back(s.ColumnString(0));
  }

  return web_app_ids;
}

bool PaymentMethodManifestTable::AddSecurePaymentConfirmationInstrument(
    const SecurePaymentConfirmationInstrument& instrument) {
  if (!instrument.IsValid())
    return false;

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  {
    // Check for credential identifier reuse by a different relying party.
    sql::Statement s0(
        db_->GetUniqueStatement("SELECT label "
                                "FROM secure_payment_confirmation_instrument "
                                "WHERE credential_id=? "
                                "AND relying_party_id<>?"));
    int index = 0;
    if (!s0.BindBlob(index++, instrument.credential_id.data(),
                     instrument.credential_id.size()))
      return false;

    if (!s0.BindString(index++, instrument.relying_party_id))
      return false;

    if (s0.Step())
      return false;
  }

  {
    sql::Statement s1(db_->GetUniqueStatement(
        "DELETE FROM secure_payment_confirmation_instrument "
        "WHERE credential_id=?"));
    if (!s1.BindBlob(0, instrument.credential_id.data(),
                     instrument.credential_id.size()))
      return false;

    if (!s1.Run())
      return false;
  }

  {
    sql::Statement s2(db_->GetUniqueStatement(
        "INSERT INTO secure_payment_confirmation_instrument "
        "(credential_id, relying_party_id, label, icon) "
        "VALUES (?, ?, ?, ?)"));
    int index = 0;
    if (!s2.BindBlob(index++, instrument.credential_id.data(),
                     instrument.credential_id.size()))
      return false;

    if (!s2.BindString(index++, instrument.relying_party_id))
      return false;

    if (!s2.BindString16(index++, instrument.label))
      return false;

    if (!s2.BindBlob(index++, instrument.icon.data(), instrument.icon.size()))
      return false;

    if (!s2.Run())
      return false;
  }

  if (!transaction.Commit())
    return false;

  return true;
}

std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>>
PaymentMethodManifestTable::GetSecurePaymentConfirmationInstruments(
    std::vector<std::vector<uint8_t>> credential_ids) {
  std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>> instruments;
  sql::Statement s(
      db_->GetUniqueStatement("SELECT relying_party_id, label, icon "
                              "FROM secure_payment_confirmation_instrument "
                              "WHERE credential_id=?"));
  // The `credential_id` temporary variable is not `const` because of the
  // `std::move()` on line 231.
  for (auto& credential_id : credential_ids) {
    s.Reset(true);
    if (credential_id.empty())
      continue;

    if (!s.BindBlob(0, credential_id.data(), credential_id.size()))
      continue;

    if (!s.Step())
      continue;

    auto instrument = std::make_unique<SecurePaymentConfirmationInstrument>();
    instrument->credential_id = std::move(credential_id);

    int index = 0;
    instrument->relying_party_id = s.ColumnString(index++);
    instrument->label = s.ColumnString16(index++);
    if (!s.ColumnBlobAsVector(index++, &instrument->icon))
      continue;

    if (!instrument->IsValid())
      continue;

    instruments.push_back(std::move(instrument));
  }

  return instruments;
}

}  // namespace payments

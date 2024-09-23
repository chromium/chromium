// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_method_manifest_table.h"

#include <time.h>

#include <string>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata/common/web_database.h"
#include "content/public/common/content_features.h"
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
  if (!db()->Execute("CREATE TABLE IF NOT EXISTS payment_method_manifest ( "
                     "expire_date INTEGER NOT NULL DEFAULT 0, "
                     "method_name VARCHAR, "
                     "web_app_id VARCHAR)")) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // The `credential_id` column is 20 bytes for UbiKey on Linux, but the size
  // can vary for different authenticators. The relatively small sizes make it
  // OK to make `credential_id` the primary key.
  if (!db()->Execute(
          "CREATE TABLE IF NOT EXISTS secure_payment_confirmation_instrument ( "
          "credential_id BLOB NOT NULL PRIMARY KEY, "
          "relying_party_id VARCHAR NOT NULL, "
          "label VARCHAR NOT NULL, "
          "icon BLOB NOT NULL)")) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (!db()->DoesColumnExist("secure_payment_confirmation_instrument",
                             "date_created")) {
    if (!db()->Execute(
            "ALTER TABLE secure_payment_confirmation_instrument ADD COLUMN "
            "date_created INTEGER NOT NULL DEFAULT 0")) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  }

  if (!db()->DoesColumnExist("secure_payment_confirmation_instrument",
                             "user_id")) {
    if (!db()->Execute(
            "ALTER TABLE secure_payment_confirmation_instrument ADD COLUMN "
            "user_id BLOB")) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  }

  return true;
}

bool PaymentMethodManifestTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  return true;
}

void PaymentMethodManifestTable::RemoveExpiredData() {
  const base::Time now_date_in_seconds = base::Time::NowFromSystemTime();
  sql::Statement s(db()->GetUniqueStatement(
      "DELETE FROM payment_method_manifest WHERE expire_date < ?"));
  s.BindTime(0, now_date_in_seconds);
  s.Run();
}

bool PaymentMethodManifestTable::ClearSecurePaymentConfirmationCredentials(
    base::Time begin,
    base::Time end) {
  sql::Statement s(db()->GetUniqueStatement(
      "DELETE FROM secure_payment_confirmation_instrument WHERE (date_created "
      ">= ? AND date_created < ?) OR (date_created = 0)"));
  s.BindTime(0, begin);
  s.BindTime(1, end);
  return s.Run();
}

bool PaymentMethodManifestTable::AddManifest(
    const std::string& payment_method,
    const std::vector<std::string>& web_app_ids) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;

  sql::Statement s1(db()->GetUniqueStatement(
      "DELETE FROM payment_method_manifest WHERE method_name=?"));
  s1.BindString(0, payment_method);
  if (!s1.Run())
    return false;

  sql::Statement s2(
      db()->GetUniqueStatement("INSERT INTO payment_method_manifest "
                               "(expire_date, method_name, web_app_id) "
                               "VALUES (?, ?, ?)"));
  const base::Time expire_date =
      base::Time::FromTimeT(base::Time::NowFromSystemTime().ToTimeT() +
                            PAYMENT_METHOD_MANIFEST_VALID_TIME_IN_SECONDS);
  for (const auto& id : web_app_ids) {
    int index = 0;
    s2.BindTime(index++, expire_date);
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
      db()->GetUniqueStatement("SELECT web_app_id "
                               "FROM payment_method_manifest "
                               "WHERE method_name=?"));
  s.BindString(0, payment_method);

  while (s.Step()) {
    web_app_ids.emplace_back(s.ColumnString(0));
  }

  return web_app_ids;
}

bool PaymentMethodManifestTable::AddSecurePaymentConfirmationCredential(
    const SecurePaymentConfirmationCredential& credential) {
  if (!credential.IsValidNewCredential())
    return false;

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;

  {
    // Check for credential identifier reuse by a different relying party.
    sql::Statement s0(
        db()->GetUniqueStatement("SELECT label "
                                 "FROM secure_payment_confirmation_instrument "
                                 "WHERE credential_id=? "
                                 "AND relying_party_id<>?"));
    int index = 0;
    s0.BindBlob(index++, credential.credential_id);
    s0.BindString(index++, credential.relying_party_id);
    if (s0.Step())
      return false;
  }

  {
    sql::Statement s1(db()->GetUniqueStatement(
        "DELETE FROM secure_payment_confirmation_instrument "
        "WHERE credential_id=?"));
    s1.BindBlob(0, credential.credential_id);

    if (!s1.Run())
      return false;
  }

  {
    // The system authenticator will overwrite a discoverable credential with
    // the same relying party and user ID, so we also clear any such credential.
    sql::Statement s2(db()->GetUniqueStatement(
        "DELETE FROM secure_payment_confirmation_instrument "
        "WHERE relying_party_id=? "
        "AND user_id=?"));
    int index = 0;
    s2.BindString(index++, credential.relying_party_id);
    s2.BindBlob(index++, credential.user_id);

    if (!s2.Run())
      return false;
  }

  {
    sql::Statement s3(db()->GetUniqueStatement(
        "INSERT INTO secure_payment_confirmation_instrument "
        "(credential_id, relying_party_id, user_id, label, icon, date_created) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    int index = 0;
    s3.BindBlob(index++, credential.credential_id);
    s3.BindString(index++, credential.relying_party_id);
    s3.BindBlob(index++, credential.user_id);
    s3.BindString(index++, std::string());
    s3.BindBlob(index++, std::vector<uint8_t>());
    s3.BindTime(index++, base::Time::Now());

    if (!s3.Run())
      return false;
  }

  if (!transaction.Commit())
    return false;

  return true;
}

std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
PaymentMethodManifestTable::GetSecurePaymentConfirmationCredentials(
    std::vector<std::vector<uint8_t>> credential_ids,
    const std::string& relying_party_id) {
  std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>> credentials;
  sql::Statement s(
      db()->GetUniqueStatement("SELECT relying_party_id, user_id "
                               "FROM secure_payment_confirmation_instrument "
                               "WHERE credential_id=? "
                               "AND relying_party_id=?"));
  // The `credential_id` temporary variable is not `const` because it is
  // std::move()'d into the credential below.
  for (auto& credential_id : credential_ids) {
    s.Reset(true);
    if (credential_id.empty())
      continue;

    s.BindBlob(0, credential_id);
    s.BindString(1, relying_party_id);

    if (!s.Step())
      continue;

    auto credential = std::make_unique<SecurePaymentConfirmationCredential>();
    credential->credential_id = std::move(credential_id);

    int index = 0;
    credential->relying_party_id = s.ColumnString(index++);
    s.ColumnBlobAsVector(index++, &(credential->user_id));

    if (!credential->IsValid())
      continue;

    credentials.push_back(std::move(credential));
  }

  return credentials;
}

bool PaymentMethodManifestTable::ExecuteForTest(const base::cstring_view sql) {
  return db()->Execute(sql);
}

bool PaymentMethodManifestTable::RazeForTest() {
  return db()->Raze();
}

bool PaymentMethodManifestTable::DoesColumnExistForTest(
    const base::cstring_view table_name,
    const base::cstring_view column_name) {
  return db()->DoesColumnExist(table_name, column_name);
}

}  // namespace payments

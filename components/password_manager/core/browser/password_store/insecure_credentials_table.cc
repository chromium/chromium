// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/insecure_credentials_table.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

// Returns a compromised credentials vector from the SQL statement.
std::vector<InsecureCredential> StatementToInsecureCredential(
    sql::Statement* s) {
  std::vector<InsecureCredential> results;
  while (s->Step()) {
    int parent_key = s->ColumnInt64(0);
    std::string signon_realm = s->ColumnString(1);
    std::u16string username = s->ColumnString16(2);
    InsecureType insecurity_type = static_cast<InsecureType>(s->ColumnInt64(3));
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::Microseconds(s->ColumnInt64(4))));
    bool is_muted = !!s->ColumnInt64(5);
    bool trigger_notification_from_backend = !!s->ColumnInt64(6);
    InsecureCredential issue(
        std::move(signon_realm), std::move(username), create_time,
        insecurity_type, IsMuted(is_muted),
        TriggerBackendNotification(trigger_notification_from_backend));
    issue.parent_key = FormPrimaryKey(parent_key);
    results.emplace_back(std::move(issue));
  }
  return results;
}

}  // namespace

InsecureCredential::InsecureCredential() = default;

InsecureCredential::InsecureCredential(
    std::string signon_realm,
    std::u16string username,
    base::Time create_time,
    InsecureType insecurity_type,
    IsMuted is_muted,
    TriggerBackendNotification trigger_notification_from_backend)

    : signon_realm(std::move(signon_realm)),
      username(std::move(username)),
      create_time(create_time),
      insecure_type(insecurity_type),
      is_muted(is_muted),
      trigger_notification_from_backend(trigger_notification_from_backend) {}

InsecureCredential::InsecureCredential(const InsecureCredential& rhs) = default;

InsecureCredential::InsecureCredential(InsecureCredential&& rhs) = default;

InsecureCredential& InsecureCredential::operator=(
    const InsecureCredential& rhs) = default;

InsecureCredential& InsecureCredential::operator=(InsecureCredential&& rhs) =
    default;

InsecureCredential::~InsecureCredential() = default;

bool InsecureCredential::SameMetadata(
    const InsecurityMetadata& metadata) const {
  return create_time == metadata.create_time && is_muted == metadata.is_muted &&
         trigger_notification_from_backend ==
             metadata.trigger_notification_from_backend;
}

bool operator==(const InsecureCredential& lhs, const InsecureCredential& rhs) {
  return lhs.signon_realm == rhs.signon_realm && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time &&
         lhs.insecure_type == rhs.insecure_type &&
         *lhs.is_muted == *rhs.is_muted &&
         *lhs.trigger_notification_from_backend ==
             *rhs.trigger_notification_from_backend;
}

#define INSECURE_CREDENTIALS_TABLE "insecure_credentials"
const char InsecureCredentialsTable::kTableName[] = INSECURE_CREDENTIALS_TABLE;

void InsecureCredentialsTable::Init(sql::Database* db) {
  db_ = db;
}

bool InsecureCredentialsTable::InsertOrReplace(FormPrimaryKey parent_key,
                                               InsecureType type,
                                               InsecurityMetadata metadata) {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT OR REPLACE INTO " INSECURE_CREDENTIALS_TABLE
                     " (parent_id, "
                     "insecurity_type, create_time, is_muted, "
                     "trigger_notification_from_backend) "
                     "VALUES (?, ?, ?, ?, ?)"));
  s.BindInt(0, parent_key.value());
  s.BindInt(1, static_cast<int>(type));
  s.BindInt64(2,
              metadata.create_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  s.BindBool(3, metadata.is_muted.value());
  s.BindBool(4, metadata.trigger_notification_from_backend.value());

  bool result = s.Run();
  return result && db_->GetLastChangeCount();
}

bool InsecureCredentialsTable::RemoveRow(FormPrimaryKey parent_key,
                                         InsecureType insecure_type) {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM " INSECURE_CREDENTIALS_TABLE
                     " WHERE parent_id = ? AND insecurity_type = ?"));
  s.BindInt(0, parent_key.value());
  s.BindInt(1, static_cast<int>(insecure_type));

  bool result = s.Run();
  return result && db_->GetLastChangeCount();
}

std::vector<InsecureCredential> InsecureCredentialsTable::GetRows(
    FormPrimaryKey parent_key) const {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT parent_id, signon_realm, username_value, "
      "insecurity_type, create_time, is_muted, "
      "trigger_notification_from_backend FROM " INSECURE_CREDENTIALS_TABLE
      " INNER JOIN logins ON parent_id = logins.id "
      "WHERE parent_id = ? "));
  s.BindInt(0, *parent_key);
  return StatementToInsecureCredential(&s);
}

}  // namespace password_manager

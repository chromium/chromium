// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/insecure_credentials_table.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_sync.h"
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
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(4))));
    bool is_muted = !!s->ColumnInt64(5);
    InsecureCredential issue(std::move(signon_realm), std::move(username),
                             create_time, insecurity_type, IsMuted(is_muted));
    issue.parent_key = FormPrimaryKey(parent_key);
    results.emplace_back(std::move(issue));
  }
  return results;
}

}  // namespace

InsecureCredential::InsecureCredential() = default;

InsecureCredential::InsecureCredential(std::string signon_realm,
                                       std::u16string username,
                                       base::Time create_time,
                                       InsecureType insecurity_type,
                                       IsMuted is_muted)
    : signon_realm(std::move(signon_realm)),
      username(std::move(username)),
      create_time(create_time),
      insecure_type(insecurity_type),
      is_muted(is_muted) {}

InsecureCredential::InsecureCredential(const InsecureCredential& rhs) = default;

InsecureCredential::InsecureCredential(InsecureCredential&& rhs) = default;

InsecureCredential& InsecureCredential::operator=(
    const InsecureCredential& rhs) = default;

InsecureCredential& InsecureCredential::operator=(InsecureCredential&& rhs) =
    default;

InsecureCredential::~InsecureCredential() = default;

bool InsecureCredential::SameMetadata(
    const InsecurityMetadata& metadata) const {
  return create_time == metadata.create_time && is_muted == metadata.is_muted;
}

bool operator==(const InsecureCredential& lhs, const InsecureCredential& rhs) {
  return lhs.signon_realm == rhs.signon_realm && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time &&
         lhs.insecure_type == rhs.insecure_type &&
         *lhs.is_muted == *rhs.is_muted;
}

const char InsecureCredentialsTable::kTableName[] = "insecure_credentials";

void InsecureCredentialsTable::Init(sql::Database* db) {
  db_ = db;
}

bool InsecureCredentialsTable::AddRow(
    const InsecureCredential& compromised_credentials) {
  DCHECK(db_);
  if (compromised_credentials.signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kTableName));

  // In case there is an error, expect it to be a constraint violation.
  db_->set_error_callback(base::BindRepeating([](int error, sql::Statement*) {
    constexpr int kSqliteErrorMask = 0xFF;
    constexpr int kSqliteConstraint = 19;
    if ((error & kSqliteErrorMask) != kSqliteConstraint) {
      DLOG(ERROR) << "Got unexpected SQL error code: " << error;
    }
  }));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s (parent_id, insecurity_type, create_time, is_muted) "
          "SELECT id, ?, ?, ? FROM logins WHERE signon_realm=? AND "
          "username_value=?",
          kTableName)
          .c_str()));

  s.BindInt(0, static_cast<int>(compromised_credentials.insecure_type));
  s.BindInt64(1, compromised_credentials.create_time.ToDeltaSinceWindowsEpoch()
                     .InMicroseconds());
  s.BindBool(2, compromised_credentials.is_muted.value());
  s.BindString(3, compromised_credentials.signon_realm);
  s.BindString16(4, compromised_credentials.username);

  bool result = s.Run();
  db_->reset_error_callback();
  return result && db_->GetLastChangeCount();
}

bool InsecureCredentialsTable::InsertOrReplace(FormPrimaryKey parent_key,
                                               InsecureType type,
                                               InsecurityMetadata metadata) {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("INSERT OR REPLACE INTO %s (parent_id, "
                         "insecurity_type, create_time, is_muted) "
                         "VALUES (?, ?, ?, ?)",
                         kTableName)
          .c_str()));
  s.BindInt(0, parent_key.value());
  s.BindInt(1, static_cast<int>(type));
  s.BindInt64(2,
              metadata.create_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  s.BindBool(3, metadata.is_muted.value());

  bool result = s.Run();
  return result && db_->GetLastChangeCount();
}

bool InsecureCredentialsTable::RemoveRow(FormPrimaryKey parent_key,
                                         InsecureType insecure_type) {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "DELETE FROM %s WHERE parent_id = ? AND insecurity_type = ?",
          kTableName)
          .c_str()));
  s.BindInt(0, parent_key.value());
  s.BindInt(1, static_cast<int>(insecure_type));

  bool result = s.Run();
  return result && db_->GetLastChangeCount();
}

bool InsecureCredentialsTable::RemoveRows(
    const std::string& signon_realm,
    const std::u16string& username,
    RemoveInsecureCredentialsReason reason) {
  DCHECK(db_);
  if (signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kTableName));

  // Retrieve the rows that are to be removed to log.
  const std::vector<InsecureCredential> compromised_credentials =
      GetRows(signon_realm);
  if (compromised_credentials.empty())
    return false;

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE parent_id IN "
                         "(SELECT id FROM logins WHERE signon_realm = ? AND "
                         "username_value = ?)",
                         kTableName)
          .c_str()));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);
  return s.Run();
}

std::vector<InsecureCredential> InsecureCredentialsTable::GetRows(
    const std::string& signon_realm) const {
  DCHECK(db_);
  if (signon_realm.empty())
    return std::vector<InsecureCredential>{};

  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT parent_id, signon_realm, username_value, "
                         "insecurity_type, create_time, is_muted FROM %s "
                         "INNER JOIN logins ON parent_id = logins.id "
                         "WHERE signon_realm = ? ",
                         kTableName)
          .c_str()));
  s.BindString(0, signon_realm);
  return StatementToInsecureCredential(&s);
}

std::vector<InsecureCredential> InsecureCredentialsTable::GetRows(
    FormPrimaryKey parent_key) const {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT parent_id, signon_realm, username_value, "
                         "insecurity_type, create_time, is_muted FROM %s "
                         "INNER JOIN logins ON parent_id = logins.id "
                         "WHERE parent_id = ? ",
                         kTableName)
          .c_str()));
  s.BindInt(0, *parent_key);
  return StatementToInsecureCredential(&s);
}

std::vector<InsecureCredential> InsecureCredentialsTable::GetAllRows() {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT parent_id, signon_realm, username_value, "
                         "insecurity_type, create_time, is_muted FROM %s "
                         "INNER JOIN logins ON parent_id = logins.id",
                         kTableName)
          .c_str()));
  return StatementToInsecureCredential(&s);
}

void InsecureCredentialsTable::ReportMetrics(BulkCheckDone bulk_check_done) {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf("SELECT COUNT(*) FROM %s "
                                        "WHERE insecurity_type = ? ",
                                        kTableName)
                         .c_str()));
  s.BindInt(0, static_cast<int>(InsecureType::kLeaked));
  if (s.Step()) {
    int count = s.ColumnInt(0);
    base::UmaHistogramCounts100(
        "PasswordManager.CompromisedCredentials.CountLeaked", count);
    if (bulk_check_done) {
      base::UmaHistogramCounts100(
          "PasswordManager.CompromisedCredentials.CountLeakedAfterBulkCheck",
          count);
    }
  }

  s.Reset(true);
  s.BindInt(0, static_cast<int>(InsecureType::kPhished));
  if (s.Step()) {
    int count = s.ColumnInt(0);
    base::UmaHistogramCounts100(
        "PasswordManager.CompromisedCredentials.CountPhished", count);
  }
}

}  // namespace password_manager

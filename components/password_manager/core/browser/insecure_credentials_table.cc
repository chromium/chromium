// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/insecure_credentials_table.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

// Returns a compromised credentials vector from the SQL statement.
std::vector<CompromisedCredentials> StatementToCompromisedCredentials(
    sql::Statement* s) {
  std::vector<CompromisedCredentials> results;
  while (s->Step()) {
    int parent_key = s->ColumnInt64(0);
    std::string signon_realm = s->ColumnString(1);
    base::string16 username = s->ColumnString16(2);
    InsecureType insecurity_type = static_cast<InsecureType>(s->ColumnInt64(3));
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(4))));
    bool is_muted = !!s->ColumnInt64(5);
    CompromisedCredentials issue(std::move(signon_realm), std::move(username),
                                 create_time, insecurity_type,
                                 IsMuted(is_muted));
    issue.parent_key = FormPrimaryKey(parent_key);
    results.emplace_back(std::move(issue));
  }
  return results;
}

}  // namespace

CompromisedCredentials::CompromisedCredentials() = default;

CompromisedCredentials::CompromisedCredentials(std::string signon_realm,
                                               base::string16 username,
                                               base::Time create_time,
                                               InsecureType insecurity_type,
                                               IsMuted is_muted)
    : signon_realm(std::move(signon_realm)),
      username(std::move(username)),
      create_time(create_time),
      insecure_type(insecurity_type),
      is_muted(is_muted) {}

CompromisedCredentials::CompromisedCredentials(
    const CompromisedCredentials& rhs) = default;

CompromisedCredentials::CompromisedCredentials(CompromisedCredentials&& rhs) =
    default;

CompromisedCredentials& CompromisedCredentials::operator=(
    const CompromisedCredentials& rhs) = default;

CompromisedCredentials& CompromisedCredentials::operator=(
    CompromisedCredentials&& rhs) = default;

CompromisedCredentials::~CompromisedCredentials() = default;

bool operator==(const CompromisedCredentials& lhs,
                const CompromisedCredentials& rhs) {
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
    const CompromisedCredentials& compromised_credentials) {
  DCHECK(db_);
  if (compromised_credentials.signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kTableName));

  base::UmaHistogramEnumeration("PasswordManager.CompromisedCredentials.Add",
                                compromised_credentials.insecure_type);

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

bool InsecureCredentialsTable::RemoveRow(
    const std::string& signon_realm,
    const base::string16& username,
    RemoveInsecureCredentialsReason reason) {
  DCHECK(db_);
  if (signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kTableName));

  // Retrieve the rows that are to be removed to log.
  const std::vector<CompromisedCredentials> compromised_credentials =
      GetRows(signon_realm);
  if (compromised_credentials.empty())
    return false;
  for (const auto& compromised_credential : compromised_credentials) {
    if (username == compromised_credential.username) {
      base::UmaHistogramEnumeration(
          "PasswordManager.CompromisedCredentials.Remove",
          compromised_credential.insecure_type);
      base::UmaHistogramEnumeration(
          "PasswordManager.RemoveCompromisedCredentials.RemoveReason", reason);
    }
  }

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

std::vector<CompromisedCredentials> InsecureCredentialsTable::GetRows(
    const std::string& signon_realm) const {
  DCHECK(db_);
  if (signon_realm.empty())
    return std::vector<CompromisedCredentials>{};

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
  return StatementToCompromisedCredentials(&s);
}

std::vector<CompromisedCredentials> InsecureCredentialsTable::GetRows(
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
  return StatementToCompromisedCredentials(&s);
}

std::vector<CompromisedCredentials> InsecureCredentialsTable::GetAllRows() {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT parent_id, signon_realm, username_value, "
                         "insecurity_type, create_time, is_muted FROM %s "
                         "INNER JOIN logins ON parent_id = logins.id",
                         kTableName)
          .c_str()));
  return StatementToCompromisedCredentials(&s);
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

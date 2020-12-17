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
    std::string signon_realm = s->ColumnString(0);
    base::string16 username = s->ColumnString16(1);
    CompromiseType insecurity_type =
        static_cast<CompromiseType>(s->ColumnInt64(2));
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(3))));
    bool is_muted = !!s->ColumnInt64(4);

    results.emplace_back(std::move(signon_realm), std::move(username),
                         create_time, insecurity_type, IsMuted(is_muted));
  }
  return results;
}

}  // namespace

CompromisedCredentials::CompromisedCredentials() = default;

CompromisedCredentials::CompromisedCredentials(std::string signon_realm,
                                               base::string16 username,
                                               base::Time create_time,
                                               CompromiseType insecurity_type,
                                               IsMuted is_muted)
    : signon_realm(std::move(signon_realm)),
      username(std::move(username)),
      create_time(create_time),
      compromise_type(insecurity_type),
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
         lhs.compromise_type == rhs.compromise_type;
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
                                compromised_credentials.compromise_type);

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

  s.BindInt(0, static_cast<int>(compromised_credentials.compromise_type));
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
    RemoveCompromisedCredentialsReason reason) {
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
          compromised_credential.compromise_type);
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
      base::StringPrintf("SELECT signon_realm, username_value, "
                         "insecurity_type, create_time, is_muted FROM %s "
                         "INNER JOIN logins ON parent_id = logins.id "
                         "WHERE signon_realm = ? ",
                         kTableName)
          .c_str()));
  s.BindString(0, signon_realm);
  return StatementToCompromisedCredentials(&s);
}

bool InsecureCredentialsTable::RemoveRowsByUrlAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  const int64_t remove_begin_us =
      remove_begin.ToDeltaSinceWindowsEpoch().InMicroseconds();
  const int64_t remove_end_us =
      remove_end.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // If |url_filter| is null, remove all records in given date range.
  if (!url_filter) {
    sql::Statement s(db_->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("DELETE FROM %s WHERE "
                           "create_time >= ? AND create_time < ?",
                           kTableName)
            .c_str()));
    s.BindInt64(0, remove_begin_us);
    s.BindInt64(1, remove_end_us);
    return s.Run();
  }

  // Otherwise, filter signon_realms.
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT DISTINCT signon_realm, id FROM logins INNER JOIN "
          "%s ON parent_id = logins.id WHERE create_time >= ? AND create_time "
          "< ?",
          kTableName)
          .c_str()));
  s.BindInt64(0, remove_begin_us);
  s.BindInt64(1, remove_end_us);

  std::vector<int64_t> ids;
  while (s.Step()) {
    std::string signon_realm = s.ColumnString(0);
    int64_t id = s.ColumnInt64(1);
    if (url_filter.Run(GURL(signon_realm))) {
      ids.push_back(id);
    }
  }

  bool success = true;
  for (int64_t id : ids) {
    sql::Statement s(db_->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("DELETE FROM %s WHERE parent_id = ? "
                           "AND create_time >= ? AND create_time < ?",
                           kTableName)
            .c_str()));
    s.BindInt64(0, id);
    s.BindInt64(1, remove_begin_us);
    s.BindInt64(2, remove_end_us);
    success = success && s.Run();
  }
  return success;
}

std::vector<CompromisedCredentials> InsecureCredentialsTable::GetAllRows() {
  DCHECK(db_);
  DCHECK(db_->DoesTableExist(kTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT signon_realm, username_value, "
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
  s.BindInt(0, static_cast<int>(CompromiseType::kLeaked));
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
  s.BindInt(0, static_cast<int>(CompromiseType::kPhished));
  if (s.Step()) {
    int count = s.ColumnInt(0);
    base::UmaHistogramCounts100(
        "PasswordManager.CompromisedCredentials.CountPhished", count);
  }
}

}  // namespace password_manager

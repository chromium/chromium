// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_TABLE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "url/gurl.h"

namespace sql {
class Database;
}

namespace password_manager {

using BulkCheckDone = base::StrongAlias<class BulkCheckDoneTag, bool>;
using IsMuted = base::StrongAlias<class IsMutedTag, bool>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InsecureType {
  // If the credentials was leaked by a data breach.
  kLeaked = 0,
  // If the credentials was entered on a phishing site.
  kPhished = 1,
  // If the password is weak.
  kWeak = 2,
  // If the password is reused for other accounts.
  kReused = 3,
  kMaxValue = kReused
};

enum class RemoveInsecureCredentialsReason {
  // If the password was updated in the password store.
  kUpdate = 0,
  // If the password is removed from the password store.
  kRemove = 1,
  // If a password was considered phished on a site later marked as legitimate.
  kMarkSiteAsLegitimate = 2,
  // If the compromised credentials was updated via sync.
  kSyncUpdate = 3,
  kMaxValue = kSyncUpdate
};

// Represents information about the particular compromised credentials.
struct CompromisedCredentials {
  CompromisedCredentials();
  CompromisedCredentials(std::string signon_realm,
                         base::string16 username,
                         base::Time create_time,
                         InsecureType insecure_type,
                         IsMuted is_muted);
  CompromisedCredentials(const CompromisedCredentials& rhs);
  CompromisedCredentials(CompromisedCredentials&& rhs);
  CompromisedCredentials& operator=(const CompromisedCredentials& rhs);
  CompromisedCredentials& operator=(CompromisedCredentials&& rhs);
  ~CompromisedCredentials();

  // The primary key of an affected Login.
  FormPrimaryKey parent_key{-1};
  // The signon_realm of the website where the credentials were compromised.
  std::string signon_realm;
  // The value of the compromised username.
  base::string16 username;
  // The date when the record was created.
  base::Time create_time;
  // The type of the credentials that was compromised.
  InsecureType insecure_type = InsecureType::kLeaked;
  // Whether the problem was explicitly muted by the user.
  IsMuted is_muted{false};
  // The store in which those credentials are stored.
  PasswordForm::Store in_store = PasswordForm::Store::kNotSet;
};

bool operator==(const CompromisedCredentials& lhs,
                const CompromisedCredentials& rhs);

// Represents the 'compromised credentials' table in the Login Database.
class InsecureCredentialsTable {
 public:
  static const char kTableName[];

  InsecureCredentialsTable() = default;
  ~InsecureCredentialsTable() = default;

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Adds information about the compromised credentials. Returns true
  // if the SQL completed successfully and an item was created.
  bool AddRow(const CompromisedCredentials& compromised_credentials);

  // Removes information about the credentials compromised for |username| on
  // |signon_realm|. |reason| is the reason why the credentials is removed from
  // the table. Returns true if the SQL completed successfully.
  // Also logs the compromise type in UMA.
  bool RemoveRow(const std::string& signon_realm,
                 const base::string16& username,
                 RemoveInsecureCredentialsReason reason);

  // Gets all the rows in the database for |signon_realm|.
  std::vector<CompromisedCredentials> GetRows(
      const std::string& signon_realm) const;

  // Gets all the rows in the database for |parent_key|.
  std::vector<CompromisedCredentials> GetRows(FormPrimaryKey parent_key) const;

  // Returns all compromised credentials from the database.
  std::vector<CompromisedCredentials> GetAllRows();

  // Reports UMA metrics about the table. |bulk_check_done| means that the
  // password bulk leak check was executed in the past.
  void ReportMetrics(BulkCheckDone bulk_check_done);

 private:
  sql::Database* db_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InsecureCredentialsTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_TABLE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_TABLE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"

namespace sql {
class Database;
}

namespace password_manager {

using BulkCheckDone = util::StrongAlias<class BulkCheckDoneTag, bool>;

enum class CompromiseType {
  // If the credentials was leaked by a data breach.
  kLeaked = 0,
  // If the credentials was reused on a phishing site.
  kPhished = 1,
  kMaxValue = kPhished
};

enum class RemoveCompromisedCredentialsReason {
  // If the password was updated in the password store.
  kUpdate = 0,
  // If the password is removed from the password store.
  kRemove = 1,
  // If a password was considered phished on a site later marked as legitimate.
  kMarkSiteAsLegitimate = 2,
  kMaxValue = kMarkSiteAsLegitimate
};

// Represents information about the particular compromised credentials.
struct CompromisedCredentials {
  // The signon_realm of the website where the credentials were compromised.
  std::string signon_realm;
  // The value of the compromised username.
  base::string16 username;
  // The date when the record was created.
  base::Time create_time;
  // The type of the credentials that was compromised.
  CompromiseType compromise_type = CompromiseType::kLeaked;
  // The store in which those credentials are stored.
  PasswordForm::Store in_store = PasswordForm::Store::kNotSet;
};

bool operator==(const CompromisedCredentials& lhs,
                const CompromisedCredentials& rhs);

// Represents the 'compromised credentials' table in the Login Database.
class CompromisedCredentialsTable {
 public:
  CompromisedCredentialsTable() = default;
  ~CompromisedCredentialsTable() = default;

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Creates the compromised credentials table if it doesn't exist.
  bool CreateTableIfNecessary();

  // Adds information about the compromised credentials. Returns true if the SQL
  // completed successfully.
  bool AddRow(const CompromisedCredentials& compromised_credentials);

  // Updates the row that has |old_signon_realm| and |old_username| with
  // |new_signon_realm| and |new_username|. If the row does not exist, the
  // method will not do anything. Returns true if the SQL completed
  // successfully.
  bool UpdateRow(const std::string& new_signon_realm,
                 const base::string16& new_username,
                 const std::string& old_signon_realm,
                 const base::string16& old_username) const;

  // Removes information about the credentials compromised for |username| on
  // |signon_realm|. |reason| is the reason why the credentials is removed from
  // the table. Returns true if the SQL completed successfully.
  // Also logs the compromise type in UMA.
  bool RemoveRow(const std::string& signon_realm,
                 const base::string16& username,
                 RemoveCompromisedCredentialsReason reason);

  // Removes information about the credentials compromised for |username| and
  // |compromise_type| on |signon_realm|. |reason| is the reason why the
  // credentials is removed from the table. Returns true if the SQL completed
  // successfully. Also logs the compromise type in UMA.
  bool RemoveRowByCompromiseType(const std::string& signon_realm,
                                 const base::string16& username,
                                 const CompromiseType& compromise_type,
                                 RemoveCompromisedCredentialsReason reason);

  // Gets all the rows in the database for the |username| and |signon_realm|.
  std::vector<CompromisedCredentials> GetRows(
      const std::string& signon_realm,
      const base::string16& username) const;

  // Gets all the rows in the database for |signon_realm|.
  std::vector<CompromisedCredentials> GetRows(
      const std::string& signon_realm) const;

  // Removes all compromised credentials created between |remove_begin|
  // inclusive and |remove_end| exclusive. If |url_filter| is not null, only
  // compromised credentials for matching signon_realms are removed. Returns
  // true if the SQL completed successfully.
  bool RemoveRowsByUrlAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end);

  // Returns all compromised credentials from the database.
  std::vector<CompromisedCredentials> GetAllRows();

  // Reports UMA metrics about the table. |bulk_check_done| means that the
  // password bulk leak check was executed in the past.
  void ReportMetrics(BulkCheckDone bulk_check_done);

 private:
  // Gets the row in the database for the |username|, |signon_realm|, and
  // |compromise_type|.
  std::vector<CompromisedCredentials> GetRowByCompromiseType(
      const std::string& signon_realm,
      const base::string16& username,
      const CompromiseType& compromise_type) const;

  sql::Database* db_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CompromisedCredentialsTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_TABLE_H_

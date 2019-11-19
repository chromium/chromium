// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_TABLE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace sql {
class Database;
}

namespace password_manager {

enum class CompromiseType {
  // If the credentials was leaked by a data breach.
  kLeaked = 0,
  // If the credentials was reused on a phishing site.
  kPhished = 1,
  kMaxValue = kPhished
};

// Represents information about the particular compromised credentials.
struct CompromisedCredentials {
  CompromisedCredentials(GURL url,
                         base::string16 username,
                         base::Time create_time,
                         CompromiseType compromise_type)
      : url(std::move(url)),
        username(std::move(username)),
        create_time(create_time),
        compromise_type(compromise_type) {}

  // The url of the website where the credentials were compromised.
  GURL url;
  // The value of the compromised username.
  base::string16 username;
  // The date when the record was created.
  base::Time create_time;
  // The type of the credentials that was compromised.
  CompromiseType compromise_type;
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

  // Removes information about the credentials compromised for |username| on
  // |url|. Returns true if the SQL completed successfully.
  // TODO(1015671): Use |compromise_type| as a param.
  bool RemoveRow(const GURL& url, const base::string16& username);

  // Removes all compromised credentials created between |remove_begin|
  // inclusive and |remove_end| exclusive. If |url_filter| is not null, only
  // compromised credentials for matching urls are removed. Returns true if the
  // SQL completed successfully.
  // TODO(1015671): Filter by |compromise_type|.
  bool RemoveRowsByUrlAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end);

  // Returns all compromised credentials from the database.
  std::vector<CompromisedCredentials> GetAllRows();

 private:
  sql::Database* db_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CompromisedCredentialsTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_TABLE_H_

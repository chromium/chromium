// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_INSECURE_CREDENTIALS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_INSECURE_CREDENTIALS_TABLE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "url/gurl.h"

namespace sql {
class Database;
}

namespace password_manager {

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
struct InsecureCredential {
  InsecureCredential();
  InsecureCredential(
      std::string signon_realm,
      std::u16string username,
      base::Time create_time,
      InsecureType insecure_type,
      IsMuted is_muted,
      TriggerBackendNotification trigger_notification_from_backend);
  InsecureCredential(const InsecureCredential& rhs);
  InsecureCredential(InsecureCredential&& rhs);
  InsecureCredential& operator=(const InsecureCredential& rhs);
  InsecureCredential& operator=(InsecureCredential&& rhs);
  ~InsecureCredential();

  bool SameMetadata(const InsecurityMetadata& metadata) const;

  // The primary key of an affected Login.
  FormPrimaryKey parent_key{-1};
  // The signon_realm of the website where the credentials were compromised.
  std::string signon_realm;
  // The value of the compromised username.
  std::u16string username;
  // The date when the record was created.
  base::Time create_time;
  // The type of the credentials that was compromised.
  InsecureType insecure_type = InsecureType::kLeaked;
  // Whether the problem was explicitly muted by the user.
  IsMuted is_muted{false};
  // Whether the backend should trigger a notification for this insecure
  // credential. Set to true if the user hasn't been notified in the client
  // (e.g. via a leak warning).
  TriggerBackendNotification trigger_notification_from_backend{false};
  // The store in which those credentials are stored.
  PasswordForm::Store in_store = PasswordForm::Store::kNotSet;
};

bool operator==(const InsecureCredential& lhs, const InsecureCredential& rhs);

// Represents the 'compromised credentials' table in the Login Database.
class InsecureCredentialsTable {
 public:
  static const char kTableName[];

  InsecureCredentialsTable() = default;

  InsecureCredentialsTable(const InsecureCredentialsTable&) = delete;
  InsecureCredentialsTable& operator=(const InsecureCredentialsTable&) = delete;

  ~InsecureCredentialsTable() = default;

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Adds information about the insecure credential if it doesn't exist.
  // If it does, it removes the previous entry and adds the new one.
  bool InsertOrReplace(FormPrimaryKey parent_key,
                       InsecureType type,
                       InsecurityMetadata metadata);

  // Removes the row corresponding to |parent_key| and |insecure_type|.
  bool RemoveRow(FormPrimaryKey parent_key, InsecureType insecure_type);

  // Gets all the rows in the database for |parent_key|.
  std::vector<InsecureCredential> GetRows(FormPrimaryKey parent_key) const;

 private:
  raw_ptr<sql::Database> db_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_INSECURE_CREDENTIALS_TABLE_H_

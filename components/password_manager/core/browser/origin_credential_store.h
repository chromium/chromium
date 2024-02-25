// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

struct PasswordForm;

// Encapsulates the data from the password manager backend as used by the UI.
class UiCredential {
 public:
  UiCredential(std::u16string username,
               std::u16string password,
               url::Origin origin,
               password_manager_util::GetLoginMatchType match_type,
               base::Time last_used);
  UiCredential(const PasswordForm& form, const url::Origin& affiliated_origin);
  UiCredential(UiCredential&&);
  UiCredential(const UiCredential&);
  UiCredential& operator=(UiCredential&&);
  UiCredential& operator=(const UiCredential&);
  ~UiCredential();

  const std::u16string& username() const { return username_; }

  const std::u16string& password() const { return password_; }

  const url::Origin& origin() const { return origin_; }

  // Domain or App name displayed in the UI for affiliated or PSL matches.
  const std::string& display_name() const { return display_name_; }

  password_manager_util::GetLoginMatchType match_type() const {
    return match_type_;
  }

  base::Time last_used() const { return last_used_; }

  bool is_shared() const { return is_shared_; }

  const std::u16string& sender_name() const { return sender_name_; }

  const GURL& sender_profile_image_url() const {
    return sender_profile_image_url_;
  }

  bool sharing_notification_displayed() const {
    return sharing_notification_displayed_;
  }

 private:
  std::u16string username_;
  std::u16string password_;
  url::Origin origin_;
  std::string display_name_;
  password_manager_util::GetLoginMatchType match_type_;
  base::Time last_used_;
  bool is_shared_ = false;
  std::u16string sender_name_;
  GURL sender_profile_image_url_;
  bool sharing_notification_displayed_ = false;
};

bool operator==(const UiCredential& lhs, const UiCredential& rhs);

std::ostream& operator<<(std::ostream& os, const UiCredential& credential);

// This class stores credential pairs originating from the same origin. The
// store is supposed to be unique per origin per tab. It is designed to share
// credentials without creating unnecessary copies.
class OriginCredentialStore {
 public:
  enum class BlocklistedStatus {
    // The origin was not blocklisted at the moment this store was initialized.
    kNeverBlocklisted,
    // The origin was blocklisted when the store was initialized, but it isn't
    // currently blocklisted.
    kWasBlocklisted,
    // The origin is currently blocklisted.
    kIsBlocklisted
  };

  explicit OriginCredentialStore(url::Origin origin);
  OriginCredentialStore(const OriginCredentialStore&) = delete;
  OriginCredentialStore& operator=(const OriginCredentialStore&) = delete;
  ~OriginCredentialStore();

  // Saves credentials so that they can be used in the UI.
  void SaveCredentials(std::vector<UiCredential> credentials);

  // Returns references to the held credentials (or an empty set if there aren't
  // any).
  base::span<const UiCredential> GetCredentials() const;

  // Saved credentials that have been received via the password sharing feature
  // and not yet notified to the user. This is important to mark them notified
  // upon user interaction with the UI.
  void SaveUnnotifiedSharedCredentials(std::vector<PasswordForm> credentials);

  // Returns references to the held unnotified shared credentials (or an empty
  // set if there aren't any).
  base::span<const PasswordForm> GetUnnotifiedSharedCredentials() const;

  // Sets the blocklisted status. The possible transitions are:
  // (*, is_blocklisted = true) -> kIsBlocklisted
  // ((kIsBlocklisted|kWasBlocklisted), is_blocklisted = false)
  //      -> kWasBlocklisted
  // (kNeverBlocklisted, is_blocklisted = false) -> kNeverBlocklisted
  void SetBlocklistedStatus(bool is_blocklisted);

  // Returns the blacklsited status for |origin_|.
  BlocklistedStatus GetBlocklistedStatus() const;

  // Removes all credentials from the store.
  void ClearCredentials();

  // Returns the origin that this store keeps credentials for.
  const url::Origin& origin() const { return origin_; }

 private:
  // Contains all previously stored of credentials.
  std::vector<UiCredential> credentials_;

  // Contains all credentials that have been received via the password sharing
  // feature and not yet notified to the user.
  std::vector<PasswordForm> unnotified_shared_credentials_;

  // The blocklisted status for |origin_|.
  // Used to know whether unblocklisting UI needs to be displayed and what
  // state it should display;
  BlocklistedStatus blocklisted_status_ = BlocklistedStatus::kNeverBlocklisted;

  // The origin which all stored passwords are related to.
  const url::Origin origin_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_

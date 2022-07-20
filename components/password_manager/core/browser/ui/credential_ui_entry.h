// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// Simple struct that represents an entry inside Settings UI. Allows implicit
// construction from PasswordForm for convenience. A single entry might
// correspond to multiple PasswordForms.
struct CredentialUIEntry {
  struct Less {
    bool operator()(const CredentialUIEntry& lhs,
                    const CredentialUIEntry& rhs) const;
  };

  CredentialUIEntry();
  explicit CredentialUIEntry(const PasswordForm& form);
  CredentialUIEntry(const CredentialUIEntry& other);
  CredentialUIEntry(CredentialUIEntry&& other);
  ~CredentialUIEntry();

  CredentialUIEntry& operator=(const CredentialUIEntry& other);
  CredentialUIEntry& operator=(CredentialUIEntry&& other);

  // The "Realm" for the sign-on. This is scheme, host, port for SCHEME_HTML.
  // Dialog based forms also contain the HTTP realm. Android based forms will
  // contain a string of the form "android://<hash of cert>@<package name>"
  std::string signon_realm;

  // An URL consists of the scheme, host, port and path; the rest is stripped.
  // This is the primary data used by the PasswordManager to decide (in
  // longest matching prefix fashion) whether or not a given PasswordForm
  // result from the database is a good fit for a particular form on a page.
  GURL url;

  // The web realm affiliated with the Android application, if the it is an
  // Android credential. Otherwise, the string is empty.
  std::string affiliated_web_realm;

  // The display name (e.g. Play Store name) of the Android application if
  // it is an Android credential. Otherwise, the string is empty.
  std::string app_display_name;

  // The current username.
  std::u16string username;

  // The current password.
  std::u16string password;

  // The origin of identity provider used for federated login.
  url::Origin federation_origin;

  // Indicates the stores where the credential is stored.
  base::flat_set<PasswordForm::Store> stored_in;

  // A mapping from the credential insecurity type (e.g. leaked, phished),
  // to its metadata (e.g. time it was discovered, whether alerts are muted).
  base::flat_map<InsecureType, InsecurityMetadata> password_issues;

  // Attached note to the credential. This is a single entry since settings UI
  // currently supports manipulation of one note only with an empty
  // `unique_display_name`. The storage layer however supports multiple-notes
  // for forward compatibility.
  PasswordNote note;

  // Tracks if the user opted to never remember passwords for this website.
  bool blocked_by_user;

  // Indicates when the credential was last used by the user to login to the
  // site. Defaults to |date_created|.
  base::Time last_used_time;

  // Information about password insecurities.
  bool IsLeaked() const;
  bool IsPhished() const;

  const base::Time GetLastLeakedOrPhishedTime() const;
};

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_

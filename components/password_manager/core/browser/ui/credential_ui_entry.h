// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

using DisplayName = base::StrongAlias<class DisplayNameTag, std::string>;
using SignonRealm = base::StrongAlias<class SignonRealmTag, std::string>;
using AffiliatedWebRealm =
    base::StrongAlias<class AffiliatedWebRealmTag, std::string>;

// CredentialUIEntry is converted to represent a group of credentials with the
// same username and password and are under the same affiliation (for example:
// apple.com and apple.de). CredentialFacet is a simple struct to keep track of
// each credential's display name, url and sign-on realm.
struct CredentialFacet {
  CredentialFacet();
  explicit CredentialFacet(DisplayName display_name,
                           GURL url,
                           SignonRealm signon_realm,
                           AffiliatedWebRealm affiliated_web_realm);
  ~CredentialFacet();
  CredentialFacet(const CredentialFacet& other);
  CredentialFacet(CredentialFacet&& other);
  CredentialFacet& operator=(const CredentialFacet& other);
  CredentialFacet& operator=(CredentialFacet&& other);

  // The display name for the website or the Android application.
  std::string display_name;

  // An URL consists of the scheme, host, port and path; the rest is stripped.
  // This is the primary data used by the PasswordManager to decide (in
  // longest matching prefix fashion) whether or not a given PasswordForm
  // result from the database is a good fit for a particular form on a page.
  GURL url;

  // The "Realm" for the sign-on. Please refer to the PasswordSpecifics
  // documentation for more details.
  std::string signon_realm;

  // The web realm affiliated with the Android application, if the it is an
  // Android credential. Otherwise, the string is empty.
  std::string affiliated_web_realm;
};

// Simple struct that represents an entry inside Settings UI. Allows implicit
// construction from PasswordForm for convenience. A single entry might
// correspond to multiple PasswordForms.
// TODO(crbug.com/40872079): Use class here instead of struct.
struct CredentialUIEntry {
  // Structure which represents affiliated domain and can be used by the UI to
  // display affiliated domains as links.
  struct DomainInfo {
    // A human readable version of the URL of the credential's origin. For
    // android credentials this is usually the app name.
    std::string name;

    // The URL that will be linked to when an entry is clicked.
    GURL url;

    // signon_realm of a corresponding PasswordForm.
    std::string signon_realm;
  };

  struct Less {
    bool operator()(const CredentialUIEntry& lhs,
                    const CredentialUIEntry& rhs) const;
  };

  CredentialUIEntry();
  explicit CredentialUIEntry(const PasswordForm& form);
  explicit CredentialUIEntry(const std::vector<PasswordForm>& forms);
  explicit CredentialUIEntry(const PasskeyCredential& passkey);
  explicit CredentialUIEntry(
      const CSVPassword& csv_password,
      PasswordForm::Store to_store = PasswordForm::Store::kProfileStore);
  CredentialUIEntry(const CredentialUIEntry& other);
  CredentialUIEntry(CredentialUIEntry&& other);
  ~CredentialUIEntry();

  CredentialUIEntry& operator=(const CredentialUIEntry& other);
  CredentialUIEntry& operator=(CredentialUIEntry&& other);

  // If this is a passkey, a non empty credential id as a byte string. Empty
  // otherwise.
  // https://w3c.github.io/webauthn/#credential-id
  std::vector<uint8_t> passkey_credential_id;

  // List of facets represented by this entry which contains the display name,
  // url and sign-on realm of a credential.
  std::vector<CredentialFacet> facets;

  // The current username.
  std::u16string username;

  // The user's display name, if this is a passkey. Always empty otherwise.
  std::u16string user_display_name;

  // The current password.
  std::u16string password;

  // The origin of identity provider used for federated login.
  url::SchemeHostPort federation_origin;

  // The creation time, if this is a passkey, nullopt otherwise.
  std::optional<base::Time> creation_time;

  // Indicates the stores where the credential is stored.
  base::flat_set<PasswordForm::Store> stored_in;

  // A mapping from the credential insecurity type (e.g. leaked, phished),
  // to its metadata (e.g. time it was discovered, whether alerts are muted).
  base::flat_map<InsecureType, InsecurityMetadata> password_issues;

  // Attached note to the credential. This is a single entry since settings UI
  // currently supports manipulation of one note only with an empty
  // `unique_display_name`. The storage layer however supports multiple-notes
  // for forward compatibility.
  std::u16string note;

  // Tracks if the user opted to never remember passwords for this website.
  bool blocked_by_user = false;

  // Indicates when the credential was last used by the user to login to the
  // site. Defaults to |date_created|.
  base::Time last_used_time;

  // Information about password insecurities.
  bool IsLeaked() const;
  bool IsPhished() const;
  bool IsWeak() const;
  bool IsReused() const;
  bool IsMuted() const;

  const base::Time GetLastLeakedOrPhishedTime() const;

  // Returns the first display name among all the display names in the facets
  // associated with this entry.
  std::string GetDisplayName() const;

  // Returns the first sign-on realm among all the sign-on realms in the facets
  // associated with this entry.
  std::string GetFirstSignonRealm() const;

  // Returns the first affiliated web realm among all the affiliated web realms
  // in the facets associated with this entry.
  std::string GetAffiliatedWebRealm() const;

  // Returns the first URL among all the URLs in the facets associated with this
  // entry.
  GURL GetURL() const;

  // Returns the URL which allows to change the password of compromised
  // credentials. Can be null for Android credentials.
  std::optional<GURL> GetChangePasswordURL() const;

  // Returns a vector of pairs, where the first element is formatted string
  // representing website or an Android application and a second parameter is a
  // link which should be opened when item is clicked. Can be used by the UI to
  // display all the affiliated domains.
  std::vector<DomainInfo> GetAffiliatedDomains() const;
};

// Creates key for sorting password or password exception entries. The key is
// eTLD+1 followed by the reversed list of domains (e.g.
// secure.accounts.example.com => example.com.com.example.accounts.secure) and
// the scheme. If |form| is not blocklisted, username, password and federation
// are appended to the key. If not, no further information is added. For Android
// credentials the canocial spec is included.
// TODO(vsemeniuk): find a better name for this function.
std::string CreateSortKey(const CredentialUIEntry& credential);

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs);
bool operator!=(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs);
bool operator<(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs);

// Returns true when the credential is either leaked or phished.
bool IsCompromised(const CredentialUIEntry& credential);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_

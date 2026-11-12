// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_

#include <compare>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/password_string.h"

namespace password_manager {

class PasskeyCredential;

using DisplayName = base::StrongAlias<class DisplayNameTag, std::string>;
using SignonRealm = base::StrongAlias<class SignonRealmTag, std::string>;
using AffiliatedWebRealm =
    base::StrongAlias<class AffiliatedWebRealmTag, std::string>;

// Non-secret fields used to sort credentials and to find small buckets of
// credentials that may be equal. Passwords must be compared separately within
// a bucket.
struct CredentialSortKey {
  // The formatted web origin or reversed Android package name.
  std::string sort_origin;

  // The canonical Android facet. Empty for web credentials.
  std::string android_facet;

  std::string scheme;
  bool blocked_by_user = false;
  std::u16string username;
  std::string federation_host;
  std::u16string passkey_display_name;
  std::vector<uint8_t> passkey_credential_id;

  friend bool operator==(const CredentialSortKey&,
                         const CredentialSortKey&) = default;
  friend auto operator<=>(const CredentialSortKey&,
                          const CredentialSortKey&) = default;
};

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

  friend bool operator==(const CredentialFacet& lhs,
                         const CredentialFacet& rhs) = default;
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

    friend bool operator==(const DomainInfo& lhs,
                           const DomainInfo& rhs) = default;

    friend auto operator<=>(const DomainInfo& lhs,
                            const DomainInfo& rhs) = default;
  };

  // Structure which represents a recovery password for a password changed in a
  // password change flow.
  struct BackupPasswordInfo {
    // The value of the backup password.
    std::u16string value;

    // The timestamp of when the backup password was set.
    base::Time creation_timestamp;
  };

  // Passkey-specific fields for entries representing a WebAuthn passkey.
  struct PasskeyInfo {
    // Non-empty credential id as a byte string.
    // https://w3c.github.io/webauthn/#credential-id
    std::vector<uint8_t> credential_id;

    // The user's display name.
    std::u16string user_display_name;

    // Indicates that the credential was marked for deletion (e.g. by a website)
    // and should be marked as such in management surfaces.
    bool hidden = false;

    // The relying party identifier.
    // https://w3c.github.io/webauthn/#relying-party-identifier
    std::string rp_id;
  };

  CredentialUIEntry();
  explicit CredentialUIEntry(const StoredCredential& credential);
  explicit CredentialUIEntry(const std::vector<StoredCredential>& credentials);
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

  // List of facets represented by this entry which contains the display name,
  // url and sign-on realm of a credential.
  std::vector<CredentialFacet> facets;

  // The current username.
  std::u16string username;

  // The current password.
  PasswordString password;

  // Recovery password for automatic password change.
  std::optional<BackupPasswordInfo> backup_password;

  // Passkey-specific metadata, if this is a passkey. `std::nullopt` otherwise.
  // When set, `credential_id` must be non-empty.
  std::optional<PasskeyInfo> passkey_info;

  // The origin of identity provider used for federated login.
  url::SchemeHostPort federation_origin;

  // The creation time of the credential. It can be `std::nullopt` in some
  // cases, e.g. when the field is not set during credential import.
  // TODO(crbug.com/501020786): Credential import / export should probably
  // operate on `StoredCredential`. Modify comment back if it's just used for
  // passkeys in settings UI.
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

  // Returns true if this entry represents a passkey.
  bool IsPasskey() const;

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

// Creates a non-secret key for sorting and bucketing credentials. Passwords
// are intentionally excluded and must be compared directly within an equal-key
// bucket. The StoredCredential overload avoids building a throwaway
// CredentialUIEntry; both overloads must produce identical keys.
CredentialSortKey CreateCredentialSortKey(const CredentialUIEntry& credential);
CredentialSortKey CreateCredentialSortKey(const StoredCredential& credential);

// Returns true if `lhs` and `rhs` represent the same logical credential
// (equal `CredentialSortKey`s and, unless `blocked_by_user` is true, equal
// passwords), ignoring metadata such as notes, stores, or insecurity issues.
bool AreCredentialsEquivalent(const CredentialUIEntry& lhs,
                              const CredentialUIEntry& rhs);

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs);
bool operator<(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs);

// Returns true when the credential is either leaked or phished.
bool IsCompromised(const CredentialUIEntry& credential);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UI_ENTRY_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_util.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;

constexpr char kPlayStoreAppPrefix[] =
    "https://play.google.com/store/apps/details?id=";

std::string GetOrigin(const url::Origin& origin) {
  return base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

CredentialSortKey CreateCredentialSortKey(
    const std::string& signon_realm,
    const GURL& credential_url,
    bool blocked_by_user,
    const std::u16string& username,
    const url::SchemeHostPort& federation_origin) {
  const FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(signon_realm);

  CredentialSortKey key;
  if (facet_uri.IsValidAndroidFacetURI()) {
    // Android credentials are sorted by reversed package name. Retain the
    // canonical facet as a separate field to distinguish app certificates.
    key.sort_origin = facet_uri.GetAndroidPackageDisplayName();
    key.android_facet = facet_uri.canonical_spec();
  } else {
    key.sort_origin =
        base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(credential_url),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  key.scheme = credential_url.GetScheme();
  key.blocked_by_user = blocked_by_user;
  if (!blocked_by_user) {
    key.username = username;
    if (federation_origin.IsValid()) {
      key.federation_host = federation_origin.host();
    }
  }
  return key;
}

}  // namespace

// CredentialFacet

CredentialFacet::CredentialFacet() = default;

CredentialFacet::CredentialFacet(DisplayName display_name,
                                 GURL url,
                                 SignonRealm signon_realm,
                                 AffiliatedWebRealm affiliated_web_realm)
    : display_name(std::move(display_name)),
      url(std::move(url)),
      signon_realm(std::move(signon_realm)),
      affiliated_web_realm(std::move(affiliated_web_realm)) {}

CredentialFacet::~CredentialFacet() = default;

CredentialFacet::CredentialFacet(const CredentialFacet& other) = default;

CredentialFacet::CredentialFacet(CredentialFacet&& other) = default;

CredentialFacet& CredentialFacet::operator=(const CredentialFacet& other) =
    default;

CredentialFacet& CredentialFacet::operator=(CredentialFacet&& other) = default;

// CredentialUIEntry

bool CredentialUIEntry::Less::operator()(const CredentialUIEntry& lhs,
                                         const CredentialUIEntry& rhs) const {
  return CreateCredentialSortKey(lhs) < CreateCredentialSortKey(rhs);
}

CredentialUIEntry::CredentialUIEntry() = default;

CredentialUIEntry::CredentialUIEntry(const StoredCredential& credential)
    : username(credential.username_value),
      password(credential.password_value.value()),
      federation_origin(credential.federation_origin),
      creation_time(credential.date_created),
      password_issues(credential.password_issues),
      note(credential.GetPasswordNote()),
      blocked_by_user(credential.blocked_by_user),
      last_used_time(credential.date_last_used) {
  if (credential.GetPasswordBackup()) {
    backup_password = {.value = credential.GetPasswordBackup().value(),
                       .creation_timestamp =
                           credential.GetPasswordBackupDateCreated().value()};
  }
  CredentialFacet facet;
  facet.display_name = credential.app_display_name;
  facet.url = credential.url;
  facet.signon_realm = credential.signon_realm;
  facet.affiliated_web_realm = credential.affiliated_web_realm;

  facets.push_back(std::move(facet));

  if (credential.IsUsingAccountStore()) {
    stored_in.insert(PasswordForm::Store::kAccountStore);
  }
  if (credential.IsUsingProfileStore()) {
    stored_in.insert(PasswordForm::Store::kProfileStore);
  }
}

CredentialUIEntry::CredentialUIEntry(
    const std::vector<StoredCredential>& credentials) {
  CHECK(!credentials.empty());

  username = credentials[0].username_value;
  password = credentials[0].password_value.value();
  federation_origin = credentials[0].federation_origin;
  password_issues = credentials[0].password_issues;
  blocked_by_user = credentials[0].blocked_by_user;
  last_used_time = credentials[0].date_last_used;
  creation_time = credentials[0].date_created;

  // For cases when the notes differ within grouped passwords (e.g: a
  // credential exists in both account and profile stores), respective notes
  // should be concatenated and linebreak used as a delimiter.
  auto unique_notes = base::MakeFlatSet<std::u16string>(
      credentials, {}, [](const auto& cred) { return cred.GetPasswordNote(); });
  unique_notes.erase(u"");
  note = base::JoinString(std::move(unique_notes).extract(), u"\n");

  // Add credential facets.
  for (const auto& credential : credentials) {
    CredentialFacet facet;
    facet.display_name = credential.app_display_name;
    facet.url = credential.url;
    facet.signon_realm = credential.signon_realm;
    facet.affiliated_web_realm = credential.affiliated_web_realm;

    facets.push_back(std::move(facet));

    if (credential.IsUsingAccountStore()) {
      stored_in.insert(PasswordForm::Store::kAccountStore);
    }
    if (credential.IsUsingProfileStore()) {
      stored_in.insert(PasswordForm::Store::kProfileStore);
    }
    // TODO(crbug.com/407501259): instead of saving the last non-empty backup,
    // consider storing all backups in the credential UI entry and create a
    // separate card for each of them.
    if (credential.GetPasswordBackup()) {
      backup_password = {.value = credential.GetPasswordBackup().value(),
                         .creation_timestamp =
                             credential.GetPasswordBackupDateCreated().value()};
    }
  }
}

CredentialUIEntry::CredentialUIEntry(const PasswordForm& form)
    : CredentialUIEntry(FromPasswordForm(form)) {}

CredentialUIEntry::CredentialUIEntry(const std::vector<PasswordForm>& forms)
    : CredentialUIEntry(FromPasswordForms(forms)) {}

CredentialUIEntry::CredentialUIEntry(const PasskeyCredential& passkey)
    : passkey_credential_id(passkey.credential_id()),
      username(base::UTF8ToUTF16(passkey.username())),
      user_display_name(base::UTF8ToUTF16(passkey.display_name())),
      creation_time(passkey.creation_time()),
      hidden(passkey.hidden()),
      rp_id(passkey.rp_id()) {
  CHECK(!passkey.credential_id().empty());
  CredentialFacet facet;
  facet.url = GURL(base::StrCat(
      {url::kHttpsScheme, url::kStandardSchemeSeparator, passkey.rp_id()}));
  facet.signon_realm =
      FacetURI::FromPotentiallyInvalidSpec(facet.url.possibly_invalid_spec())
          .potentially_invalid_spec();
  facets.push_back(std::move(facet));
}

CredentialUIEntry::CredentialUIEntry(const CSVPassword& csv_password,
                                     PasswordForm::Store to_store)
    : username(base::UTF8ToUTF16(csv_password.GetUsername())),
      password(base::UTF8ToUTF16(csv_password.GetPassword())),
      note(base::UTF8ToUTF16(csv_password.GetNote())) {
  CredentialFacet facet;
  facet.url = csv_password.GetURL().value();
  facet.signon_realm =
      affiliations::IsValidAndroidFacetURI(csv_password.GetURL().value().spec())
          ? csv_password.GetURL().value().spec()
          : GetSignonRealm(csv_password.GetURL().value());
  facets.push_back(std::move(facet));

  DCHECK_EQ(csv_password.GetParseStatus(), CSVPassword::Status::kOK);

  stored_in.insert(to_store);
}

CredentialUIEntry::CredentialUIEntry(const CredentialUIEntry& other) = default;
CredentialUIEntry::CredentialUIEntry(CredentialUIEntry&& other) = default;
CredentialUIEntry::~CredentialUIEntry() = default;

CredentialUIEntry& CredentialUIEntry::operator=(
    const CredentialUIEntry& other) = default;
CredentialUIEntry& CredentialUIEntry::operator=(CredentialUIEntry&& other) =
    default;

bool CredentialUIEntry::IsLeaked() const {
  return password_issues.contains(InsecureType::kLeaked);
}

bool CredentialUIEntry::IsPhished() const {
  return password_issues.contains(InsecureType::kPhished);
}

bool CredentialUIEntry::IsReused() const {
  return password_issues.contains(InsecureType::kReused);
}

bool CredentialUIEntry::IsWeak() const {
  return password_issues.contains(InsecureType::kWeak);
}

bool CredentialUIEntry::IsMuted() const {
  return (IsLeaked() && password_issues.at(InsecureType::kLeaked).is_muted) ||
         (IsPhished() && password_issues.at(InsecureType::kPhished).is_muted);
}

const base::Time CredentialUIEntry::GetLastLeakedOrPhishedTime() const {
  DCHECK(IsLeaked() || IsPhished());
  base::Time compromise_time;
  if (IsLeaked()) {
    compromise_time = password_issues.at(InsecureType::kLeaked).create_time;
  }
  if (IsPhished()) {
    compromise_time =
        std::max(compromise_time,
                 password_issues.at(InsecureType::kPhished).create_time);
  }
  return compromise_time;
}

std::string CredentialUIEntry::GetDisplayName() const {
  DCHECK(!facets.empty());
  return facets[0].display_name;
}

std::string CredentialUIEntry::GetFirstSignonRealm() const {
  DCHECK(!facets.empty());
  return facets[0].signon_realm;
}

std::string CredentialUIEntry::GetAffiliatedWebRealm() const {
  DCHECK(!facets.empty());
  return facets[0].affiliated_web_realm;
}

GURL CredentialUIEntry::GetURL() const {
  DCHECK(!facets.empty());
  return facets[0].url;
}

std::optional<GURL> CredentialUIEntry::GetChangePasswordURL() const {
  GURL change_password_origin;
  auto facetUri = FacetURI::FromPotentiallyInvalidSpec(GetFirstSignonRealm());

  if (facetUri.IsValidAndroidFacetURI()) {
    // Change url needs special handling for Android. Here we use
    // affiliation information instead of the origin.
    if (!GetAffiliatedWebRealm().empty()) {
      return CreateChangePasswordUrl(GURL(GetAffiliatedWebRealm()));
    }
  } else if (GetURL().is_valid()) {
    return CreateChangePasswordUrl(GetURL());
  }

  return std::nullopt;
}

std::vector<CredentialUIEntry::DomainInfo>
CredentialUIEntry::GetAffiliatedDomains() const {
  std::vector<CredentialUIEntry::DomainInfo> domains;
  std::set<std::string> unique_urls;
  CHECK(!facets.empty());
  for (const auto& facet : facets) {
    CredentialUIEntry::DomainInfo domain;
    domain.signon_realm = facet.signon_realm;
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(facet.signon_realm);
    if (facet_uri.IsValidAndroidFacetURI()) {
      domain.name = facet.display_name.empty()
                        ? facet_uri.GetAndroidPackageDisplayName()
                        : facet.display_name;
      domain.url =
          facet.affiliated_web_realm.empty()
              ? GURL(kPlayStoreAppPrefix + facet_uri.android_package_name())
              : GURL(facet.affiliated_web_realm);
    } else {
      domain.url = facet.url;
      std::string origin = GetOrigin(url::Origin::Create(facet.url));
      domain.name =
          origin.empty() ? domain.url.possibly_invalid_spec() : origin;
    }
    if (unique_urls.insert(domain.url.possibly_invalid_spec()).second) {
      domains.push_back(std::move(domain));
    }
  }
  return domains;
}

CredentialSortKey CreateCredentialSortKey(const CredentialUIEntry& credential) {
  CredentialSortKey key = CreateCredentialSortKey(
      credential.GetFirstSignonRealm(), credential.GetURL(),
      credential.blocked_by_user, credential.username,
      credential.federation_origin);
  if (!credential.passkey_credential_id.empty()) {
    key.passkey_display_name = credential.user_display_name;
    key.passkey_credential_id = credential.passkey_credential_id;
  }
  return key;
}

CredentialSortKey CreateCredentialSortKey(const StoredCredential& credential) {
  return CreateCredentialSortKey(
      credential.signon_realm, credential.url, credential.blocked_by_user,
      credential.username_value, credential.federation_origin);
}

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  return CreateCredentialSortKey(lhs) == CreateCredentialSortKey(rhs) &&
         (lhs.blocked_by_user || lhs.password == rhs.password);
}

bool operator<(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  // Intentionally does not include password. While it would be trivial to do
  // now, a follow up CL will change |password| to a |PasswordString| object,
  // which uses base::ProcessBoundString to encrypt the password in memory and
  // thus doesn't expose a |operator<|.
  return CreateCredentialSortKey(lhs) < CreateCredentialSortKey(rhs);
}

bool IsCompromised(const CredentialUIEntry& credential) {
  return credential.IsLeaked() || credential.IsPhished();
}

}  // namespace password_manager

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_util.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;

constexpr char kPlayStoreAppPrefix[] =
    "https://play.google.com/store/apps/details?id=";

constexpr char kSortKeyPartsSeparator = ' ';

std::string GetOrigin(const url::Origin& origin) {
  return base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
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
  return CreateSortKey(lhs) < CreateSortKey(rhs);
}

CredentialUIEntry::CredentialUIEntry() = default;

CredentialUIEntry::CredentialUIEntry(const PasswordForm& form)
    : username(form.username_value),
      password(form.password_value),
      federation_origin(form.federation_origin),
      password_issues(form.password_issues),
      note(form.GetNoteWithEmptyUniqueDisplayName()),
      blocked_by_user(form.blocked_by_user),
      last_used_time(form.date_last_used) {
  CredentialFacet facet;
  facet.display_name = form.app_display_name;
  facet.url = form.url;
  facet.signon_realm = form.signon_realm;
  facet.affiliated_web_realm = form.affiliated_web_realm;

  facets.push_back(std::move(facet));

  if (form.IsUsingAccountStore()) {
    stored_in.insert(PasswordForm::Store::kAccountStore);
  }
  if (form.IsUsingProfileStore()) {
    stored_in.insert(PasswordForm::Store::kProfileStore);
  }
}

CredentialUIEntry::CredentialUIEntry(const std::vector<PasswordForm>& forms) {
  CHECK(!forms.empty());

  username = forms[0].username_value;
  password = forms[0].password_value;
  federation_origin = forms[0].federation_origin;
  password_issues = forms[0].password_issues;
  blocked_by_user = forms[0].blocked_by_user;
  last_used_time = forms[0].date_last_used;

  // For cases when the notes differ within grouped passwords (e.g: a
  // credential exists in both account and profile stores), respective notes
  // should be concatenated and linebreak used as a delimiter.
  auto unique_notes =
      base::MakeFlatSet<std::u16string>(forms, {}, [](const auto& form) {
        return form.GetNoteWithEmptyUniqueDisplayName();
      });
  unique_notes.erase(u"");
  note = base::JoinString(std::move(unique_notes).extract(), u"\n");

  // Add credential facets.
  for (const auto& form : forms) {
    CredentialFacet facet;
    facet.display_name = form.app_display_name;
    facet.url = form.url;
    facet.signon_realm = form.signon_realm;
    facet.affiliated_web_realm = form.affiliated_web_realm;

    facets.push_back(std::move(facet));

    if (form.IsUsingAccountStore()) {
      stored_in.insert(PasswordForm::Store::kAccountStore);
    }
    if (form.IsUsingProfileStore()) {
      stored_in.insert(PasswordForm::Store::kProfileStore);
    }
  }
}

CredentialUIEntry::CredentialUIEntry(const PasskeyCredential& passkey)
    : passkey_credential_id(passkey.credential_id()),
      username(base::UTF8ToUTF16(passkey.username())),
      user_display_name(base::UTF8ToUTF16(passkey.display_name())),
      creation_time(passkey.creation_time()) {
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
  auto facetUri = password_manager::FacetURI::FromPotentiallyInvalidSpec(
      GetFirstSignonRealm());

  if (facetUri.IsValidAndroidFacetURI()) {
    // Change url needs special handling for Android. Here we use
    // affiliation information instead of the origin.
    if (!GetAffiliatedWebRealm().empty()) {
      return password_manager::CreateChangePasswordUrl(
          GURL(GetAffiliatedWebRealm()));
    }
  } else if (GetURL().is_valid()) {
    return password_manager::CreateChangePasswordUrl(GetURL());
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

std::string CreateSortKey(const CredentialUIEntry& credential) {
  const FacetURI facet_uri =
      FacetURI::FromPotentiallyInvalidSpec(credential.GetFirstSignonRealm());

  std::string key;
  if (facet_uri.IsValidAndroidFacetURI()) {
    // In case of Android credentials |GetShownOriginAndLinkURl| might return
    // the app display name, e.g. the Play Store name of the given application.
    // This might or might not correspond to the eTLD+1, which is why
    // |key| is set to the reversed android package name in this case,
    // e.g. com.example.android => android.example.com.
    key = facet_uri.GetAndroidPackageDisplayName() + kSortKeyPartsSeparator +
          facet_uri.canonical_spec();
  } else {
    key = base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
        url::Origin::Create(credential.GetURL()),
        url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  // Add a scheme to distinguish between http and https websites.
  key += credential.GetURL().scheme();

  if (!credential.blocked_by_user) {
    key += kSortKeyPartsSeparator + base::UTF16ToUTF8(credential.username) +
           kSortKeyPartsSeparator + base::UTF16ToUTF8(credential.password);

    key += kSortKeyPartsSeparator;
    if (credential.federation_origin.IsValid()) {
      key += credential.federation_origin.host();
    }
  }

  // Separate passwords from passkeys.
  if (!credential.passkey_credential_id.empty()) {
    key += kSortKeyPartsSeparator +
           base::UTF16ToUTF8(credential.user_display_name) +
           kSortKeyPartsSeparator +
           base::HexEncode(credential.passkey_credential_id);
  }
  return key;
}

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  return CreateSortKey(lhs) == CreateSortKey(rhs);
}

bool operator!=(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  return !(lhs == rhs);
}

bool operator<(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  return CreateSortKey(lhs) < CreateSortKey(rhs);
}

bool IsCompromised(const CredentialUIEntry& credential) {
  return credential.IsLeaked() || credential.IsPhished();
}

}  // namespace password_manager

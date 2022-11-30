// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"

namespace password_manager {

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
      blocked_by_user(form.blocked_by_user),
      last_used_time(form.date_last_used) {
  // Only one-note with an empty `unique_display_name` is supported in the
  // settings UI.
  for (const PasswordNote& n : form.notes) {
    if (n.unique_display_name.empty()) {
      note = n;
      break;
    }
  }

  CredentialFacet facet;
  facet.display_name = form.app_display_name;
  facet.url = form.url;
  facet.signon_realm = form.signon_realm;
  facet.affiliated_web_realm = form.affiliated_web_realm;

  facets.push_back(std::move(facet));

  if (form.IsUsingAccountStore())
    stored_in.insert(PasswordForm::Store::kAccountStore);
  if (form.IsUsingProfileStore())
    stored_in.insert(PasswordForm::Store::kProfileStore);
}

CredentialUIEntry::CredentialUIEntry(const std::vector<PasswordForm>& forms) {
  CHECK(!forms.empty());

  username = forms[0].username_value;
  password = forms[0].password_value;
  federation_origin = forms[0].federation_origin;
  password_issues = forms[0].password_issues;
  blocked_by_user = forms[0].blocked_by_user;
  last_used_time = forms[0].date_last_used;

  // Only one-note with an empty `unique_display_name` is supported in the
  // settings UI.
  for (const PasswordNote& n : forms[0].notes) {
    if (n.unique_display_name.empty()) {
      note = n;
      break;
    }
  }

  // Add credential facets.
  for (const auto& form : forms) {
    CredentialFacet facet;
    facet.display_name = form.app_display_name;
    facet.url = form.url;
    facet.signon_realm = form.signon_realm;
    facet.affiliated_web_realm = form.affiliated_web_realm;

    facets.push_back(std::move(facet));

    if (form.IsUsingAccountStore())
      stored_in.insert(PasswordForm::Store::kAccountStore);
    if (form.IsUsingProfileStore())
      stored_in.insert(PasswordForm::Store::kProfileStore);
  }
}

CredentialUIEntry::CredentialUIEntry(const CSVPassword& csv_password,
                                     PasswordForm::Store to_store)
    : username(base::UTF8ToUTF16(csv_password.GetUsername())),
      password(base::UTF8ToUTF16(csv_password.GetPassword())) {
  CredentialFacet facet;
  facet.url = csv_password.GetURL().value();
  facet.signon_realm =
      IsValidAndroidFacetURI(csv_password.GetURL().value().spec())
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

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  return CreateSortKey(lhs) == CreateSortKey(rhs);
}

}  // namespace password_manager

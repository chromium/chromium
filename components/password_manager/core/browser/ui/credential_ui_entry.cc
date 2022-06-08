// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#include "components/password_manager/core/browser/password_list_sorter.h"

namespace password_manager {

CredentialUIEntry::CredentialUIEntry(const PasswordForm& form)
    : signon_realm(form.signon_realm),
      url(form.url),
      affiliated_web_realm(form.affiliated_web_realm),
      app_display_name(form.app_display_name),
      username(form.username_value),
      password(form.password_value),
      federation_origin(form.federation_origin),
      password_issues(form.password_issues),
      blocked_by_user(form.blocked_by_user),
      key_(CredentialKey(CreateSortKey(form, IgnoreStore(true)))) {
  // Only one-note with an empty `unique_display_name` is supported in the
  // settings UI.
  for (const PasswordNote& n : form.notes) {
    if (n.unique_display_name.empty()) {
      note = n;
      break;
    }
  }
  if (form.IsUsingAccountStore())
    stored_in.insert(PasswordForm::Store::kAccountStore);
  if (form.IsUsingProfileStore())
    stored_in.insert(PasswordForm::Store::kProfileStore);
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

bool operator==(const CredentialUIEntry& lhs, const CredentialUIEntry& rhs) {
  return lhs.key() == rhs.key();
}

}  // namespace password_manager

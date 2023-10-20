// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_SHARING_INVITATIONS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_SHARING_INVITATIONS_H_

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_form.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace password_manager {

// IncomingSharingInvitation primary key which is used in the database.
using IncomingInvitationPrimaryKey =
    base::StrongAlias<class IncomingInvitationPrimaryKeyTag, int>;

// Represents a password sharing invitation received in the password manager.
struct IncomingSharingInvitation {
  IncomingSharingInvitation();
  IncomingSharingInvitation(const IncomingSharingInvitation& rhs);
  IncomingSharingInvitation(IncomingSharingInvitation&& rhs);
  IncomingSharingInvitation& operator=(const IncomingSharingInvitation& rhs);
  IncomingSharingInvitation& operator=(IncomingSharingInvitation&& rhs);
  ~IncomingSharingInvitation();

  // The primary key of the sharing invitation record in the logins database.
  // This is only set when the invitation has been read from the login database.
  absl::optional<IncomingInvitationPrimaryKey> primary_key;

  // Those are aligned with the counterparts in the PasswordForm struct. Refer
  // to password_form.h for documentation.
  GURL url;
  std::u16string username_element;
  std::u16string username_value;
  std::u16string password_element;
  std::string signon_realm;
  std::u16string password_value;
  PasswordForm::Scheme scheme = PasswordForm::Scheme::kHtml;
  std::u16string display_name;
  GURL icon_url;

  // Invitation metadata:
  base::Time date_created;
  std::u16string sender_email;
  std::u16string sender_display_name;
  GURL sender_profile_image_url;
};

PasswordForm IncomingSharingInvitationToPasswordForm(
    const IncomingSharingInvitation& invitation);

// For testing.
#if defined(UNIT_TEST)
bool operator==(const IncomingSharingInvitation& lhs,
                const IncomingSharingInvitation& rhs);
bool operator!=(const IncomingSharingInvitation& lhs,
                const IncomingSharingInvitation& rhs);
std::ostream& operator<<(std::ostream& os,
                         const IncomingSharingInvitation& invitation);
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_SHARING_INVITATIONS_H_

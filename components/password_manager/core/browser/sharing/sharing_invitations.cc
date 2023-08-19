// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/sharing_invitations.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

IncomingSharingInvitation::IncomingSharingInvitation() = default;
IncomingSharingInvitation::IncomingSharingInvitation(
    const IncomingSharingInvitation& rhs) = default;
IncomingSharingInvitation::IncomingSharingInvitation(
    IncomingSharingInvitation&& rhs) = default;
IncomingSharingInvitation& IncomingSharingInvitation::operator=(
    const IncomingSharingInvitation& rhs) = default;
IncomingSharingInvitation& IncomingSharingInvitation::operator=(
    IncomingSharingInvitation&& rhs) = default;
IncomingSharingInvitation::~IncomingSharingInvitation() = default;

bool operator==(const IncomingSharingInvitation& lhs,
                const IncomingSharingInvitation& rhs) {
  return

      lhs.url == lhs.url && lhs.username_element == rhs.username_element &&
      lhs.username_value == rhs.username_value &&
      lhs.password_element == rhs.password_element &&
      lhs.signon_realm == rhs.signon_realm &&
      lhs.password_value == rhs.password_value &&
      lhs.date_created == rhs.date_created &&
      lhs.sender_email == rhs.sender_email &&
      lhs.sender_display_name == rhs.sender_display_name;
}

std::ostream& operator<<(std::ostream& os,
                         const IncomingSharingInvitation& invitation) {
  return os << "IncomingSharingInvitation("
            << "\nprimary_key: "
            << invitation.primary_key.value_or(IncomingInvitationPrimaryKey(-1))
            << "\nurl: " << invitation.url
            << "\nusername_element: " << invitation.username_element
            << "\nusername_value: " << invitation.username_value
            << "\npassword_element: " << invitation.password_element
            << "\nsignon_realm: " << invitation.signon_realm
            << "\npassword_value: " << invitation.password_value
            << "\nscheme: " << static_cast<int>(invitation.scheme)
            << "\ndisplay_name: " << invitation.display_name
            << "\nicon_url: " << invitation.icon_url
            << "\ndate_created: " << invitation.date_created
            << "\nsender_email: " << invitation.sender_email
            << "\nsender_display_name: " << invitation.sender_display_name
            << "\n)\n";
}

PasswordForm IncomingSharingInvitationToPasswordForm(
    const IncomingSharingInvitation& invitation) {
  PasswordForm form;
  form.url = invitation.url;
  form.username_element = invitation.username_element;
  form.username_value = invitation.username_value;
  form.password_element = invitation.password_element;
  form.signon_realm = invitation.signon_realm;
  form.password_value = invitation.password_value;
  form.scheme = invitation.scheme;
  form.display_name = invitation.display_name;
  form.date_created = base::Time::Now();
  form.type = PasswordForm::Type::kReceivedViaSharing;
  form.sender_email = invitation.sender_email;
  form.sender_name = invitation.sender_display_name;
  form.date_received = base::Time::Now();
  form.sharing_notification_displayed = false;
  return form;
}

}  // namespace password_manager

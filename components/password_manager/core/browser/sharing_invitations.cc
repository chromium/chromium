// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing_invitations.h"

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
      lhs.sender_email == rhs.sender_email &&
      lhs.date_created == rhs.date_created;
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
            << "\nsender_email: " << invitation.sender_email
            << "\ndate_created: " << invitation.date_created << "\n)\n";
}

}  // namespace password_manager

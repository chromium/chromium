// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_sender_service_impl.h"

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"

namespace password_manager {

PasswordSenderServiceImpl::PasswordSenderServiceImpl(
    std::unique_ptr<OutgoingPasswordSharingInvitationSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  CHECK(sync_bridge_);
}

PasswordSenderServiceImpl::~PasswordSenderServiceImpl() = default;

void PasswordSenderServiceImpl::SendPassword(
    const CredentialUIEntry& credential_ui_entry,
    const PasswordRecipient& recipient) {
  // TODO(crbug.com/1455407): Implement.
}

void PasswordSenderServiceImpl::Shutdown() {}

}  // namespace password_manager

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service.h"

#include <memory>

#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"

namespace password_manager {

PasswordReceiverService::PasswordReceiverService(
    std::unique_ptr<
        password_manager::IncomingPasswordSharingInvitationSyncBridge>
        sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {}

PasswordReceiverService::~PasswordReceiverService() = default;

}  // namespace password_manager

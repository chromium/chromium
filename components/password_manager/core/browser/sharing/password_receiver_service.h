// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace password_manager {

class IncomingPasswordSharingInvitationSyncBridge;

// The per-profile service responsible for processing incoming password sharing
// invitations.
class PasswordReceiverService : public KeyedService {
 public:
  explicit PasswordReceiverService(
      std::unique_ptr<
          password_manager::IncomingPasswordSharingInvitationSyncBridge>
          sync_bridge);
  PasswordReceiverService(const PasswordReceiverService&) = delete;
  PasswordReceiverService& operator=(const PasswordReceiverService&) = delete;
  ~PasswordReceiverService() override;

 private:
  std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_

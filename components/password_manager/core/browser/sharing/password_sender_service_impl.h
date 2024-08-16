// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_IMPL_H_

#include <memory>

#include "components/password_manager/core/browser/sharing/password_sender_service.h"

namespace password_manager {

class OutgoingPasswordSharingInvitationSyncBridge;

class PasswordSenderServiceImpl : public PasswordSenderService {
 public:
  explicit PasswordSenderServiceImpl(
      std::unique_ptr<OutgoingPasswordSharingInvitationSyncBridge> sync_bridge);
  PasswordSenderServiceImpl(const PasswordSenderServiceImpl&) = delete;
  PasswordSenderServiceImpl& operator=(const PasswordSenderServiceImpl&) =
      delete;
  ~PasswordSenderServiceImpl() override;

  // PasswordSenderService implementation
  void SendPasswords(const std::vector<PasswordForm>& passwords,
                     const PasswordRecipient& recipient) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  // KeyedService (through PasswordSenderService) implementation.
  void Shutdown() override;

 private:
  const std::unique_ptr<OutgoingPasswordSharingInvitationSyncBridge>
      sync_bridge_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_IMPL_H_

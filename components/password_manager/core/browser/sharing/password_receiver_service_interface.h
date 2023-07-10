// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_INTERFACE_H_

#include "components/password_manager/core/browser/sharing/sharing_invitations.h"

namespace password_manager {

// The interface used to interact with the password receiver keyed service.
class PasswordReceiverServiceInterface {
 public:
  virtual void ProcessIncomingSharingInvitation(
      IncomingSharingInvitation invitation) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_INTERFACE_H_

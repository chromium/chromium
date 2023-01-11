// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLIENT_DATA_DELEGATE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLIENT_DATA_DELEGATE_H_

#include "base/functional/callback_forward.h"

namespace enterprise_management {
class RegisterBrowserRequest;
}  // namespace enterprise_management

namespace policy {

// Sets platform-specific fields in request protos for the DMServer.
class ClientDataDelegate {
 public:
  ClientDataDelegate() = default;
  ClientDataDelegate(const ClientDataDelegate&) = delete;
  ClientDataDelegate& operator=(const ClientDataDelegate&) = delete;
  virtual ~ClientDataDelegate() = default;

  virtual void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const = 0;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLIENT_DATA_DELEGATE_H_

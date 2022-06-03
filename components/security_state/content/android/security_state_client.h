// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_CLIENT_H_
#define COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_CLIENT_H_

#include <memory>
#include "components/security_state/content/android/security_state_model_delegate.h"

namespace security_state {
class SecurityStateClient;

void SetSecurityStateClient(SecurityStateClient* security_state_client);
SecurityStateClient* GetSecurityStateClient();

class SecurityStateClient {
 public:
  SecurityStateClient() = default;
  ~SecurityStateClient() = default;

  // Create a SecurityStateModelDelegate. This can return a nullptr.
  virtual std::unique_ptr<SecurityStateModelDelegate>
  MaybeCreateSecurityStateModelDelegate();
};
}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_CLIENT_H_

// Copyright 2020 The Chromium Authors
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
  SecurityStateClient();
  virtual ~SecurityStateClient();

  // Create a SecurityStateModelDelegate. This can return a nullptr.
  virtual std::unique_ptr<SecurityStateModelDelegate>
  MaybeCreateSecurityStateModelDelegate();

  // Returns the delegate for this client, created on first use via
  // MaybeCreateSecurityStateModelDelegate() and owned by the client. Can
  // return null.
  SecurityStateModelDelegate* GetSecurityStateModelDelegate();

 private:
  bool delegate_created_ = false;
  std::unique_ptr<SecurityStateModelDelegate> delegate_;
};

// Returns the registered client's SecurityStateModelDelegate, or null when
// no client is registered (e.g. WebView). The delegate is owned by the
// client, so replacing the client with SetSecurityStateClient() (as tests
// do) replaces the delegate along with it.
SecurityStateModelDelegate* GetSecurityStateModelDelegate();
}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_CLIENT_H_

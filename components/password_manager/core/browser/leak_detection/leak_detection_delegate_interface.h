// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_DELEGATE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_DELEGATE_INTERFACE_H_

#include "url/gurl.h"

namespace password_manager {

enum class LeakDetectionError {
  // The user isn't signed-in to Chrome.
  kNotSignIn = 0,
  // Error obtaining a token.
  kTokenRequestFailure = 1,
  // Error in hashing/encrypting for the request.
  kHashingFailure = 2,
  // Error obtaining a valid server response.
  kInvalidServerResponse = 3,
  // TODO(crbug.com/986298): add more errors.
};

// Interface with callbacks for LeakDetectionCheck. Used to get the result of
// the check.
class LeakDetectionDelegateInterface {
 public:
  LeakDetectionDelegateInterface() = default;
  virtual ~LeakDetectionDelegateInterface() = default;

  // Not copyable or movable
  LeakDetectionDelegateInterface(const LeakDetectionDelegateInterface&) =
      delete;
  LeakDetectionDelegateInterface& operator=(
      const LeakDetectionDelegateInterface&) = delete;
  LeakDetectionDelegateInterface(LeakDetectionDelegateInterface&&) = delete;
  LeakDetectionDelegateInterface& operator=(LeakDetectionDelegateInterface&&) =
      delete;

  // Called when the request is finished without error.
  // |leak| is true iff the checked credential was leaked.
  // |url| and |username| are taken from Start() for presentation in the UI.
  // Pass parameters by value because the caller can be destroyed here.
  virtual void OnLeakDetectionDone(bool is_leaked,
                                   GURL url,
                                   base::string16 username,
                                   base::string16 password) = 0;

  virtual void OnError(LeakDetectionError error) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_DELEGATE_INTERFACE_H_

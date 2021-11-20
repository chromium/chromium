// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_BROWSER_CLIENT_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_BROWSER_CLIENT_H_

#include <string>

#include "base/callback_forward.h"

namespace ash {

// A client class which provides browser access to Quick Answers.
class QuickAnswersBrowserClient {
 public:
  using GetAccessTokenCallback =
      base::OnceCallback<void(const std::string& access_token)>;

  QuickAnswersBrowserClient();
  virtual ~QuickAnswersBrowserClient();

  // Get the instance of |QuickAnswersBrowserClient|.
  static QuickAnswersBrowserClient* Get();

  // Request for the access token associated with the active user's profile.
  // Request is handled asynchronously if the token is not available.
  // AccessTokenCallbacks are invoked as soon as the token if fetched.
  // If the token is available, AccessTokenCallbacks are invoked
  // synchronously before RequestAccessToken() returns.
  virtual void RequestAccessToken(GetAccessTokenCallback callback) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_BROWSER_CLIENT_H_

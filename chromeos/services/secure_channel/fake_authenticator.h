// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_AUTHENTICATOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_AUTHENTICATOR_H_

#include "base/callback.h"
#include "chromeos/services/secure_channel/authenticator.h"

namespace chromeos {

namespace secure_channel {

// A fake implementation of Authenticator to use in tests.
class FakeAuthenticator : public Authenticator {
 public:
  FakeAuthenticator();

  FakeAuthenticator(const FakeAuthenticator&) = delete;
  FakeAuthenticator& operator=(const FakeAuthenticator&) = delete;

  ~FakeAuthenticator() override;

  // Authenticator:
  void Authenticate(AuthenticationCallback callback) override;

  AuthenticationCallback last_callback() { return std::move(last_callback_); }

 private:
  AuthenticationCallback last_callback_;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_AUTHENTICATOR_H_

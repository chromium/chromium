// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/authenticator.h"

namespace ash::secure_channel {

// static
const char Authenticator::kAuthenticationFeature[] = "auth";

Authenticator::Authenticator() = default;
Authenticator::~Authenticator() {}

void Authenticator::AddObserver(AuthenticatorObserver* observer) {
  authentication_state_observers_.AddObserver(observer);
}
void Authenticator::RemoveObserver(AuthenticatorObserver* observer) {
  authentication_state_observers_.RemoveObserver(observer);
}

void Authenticator::NotifyAuthenticationStateChanged(
    mojom::SecureChannelState secure_channel_state) {
  for (auto& observer : authentication_state_observers_) {
    observer.OnAuthenticationStateChanged(secure_channel_state);
  }
}

}  // namespace ash::secure_channel

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/fake_platform_api.h"
#include "base/logging.h"

namespace chromeos {
namespace assistant {

assistant_client::AudioInputProvider& FakePlatformApi::GetAudioInputProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::AudioOutputProvider&
FakePlatformApi::GetAudioOutputProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::AuthProvider& FakePlatformApi::GetAuthProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::FileProvider& FakePlatformApi::GetFileProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::NetworkProvider& FakePlatformApi::GetNetworkProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::SystemProvider& FakePlatformApi::GetSystemProvider() {
  NOTIMPLEMENTED();
  abort();
}

}  // namespace assistant
}  // namespace chromeos

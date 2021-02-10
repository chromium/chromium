// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/notreached.h"
#include "chromeos/services/assistant//public/cpp/migration/fake_platform_api.h"

namespace chromeos {
namespace assistant {

FakePlatformApi::FakePlatformApi() = default;
FakePlatformApi::~FakePlatformApi() = default;

assistant_client::FileProvider& FakePlatformApi::GetFileProvider() {
  NOTIMPLEMENTED();
  abort();
}

assistant_client::NetworkProvider& FakePlatformApi::GetNetworkProvider() {
  NOTIMPLEMENTED();
  abort();
}

}  // namespace assistant
}  // namespace chromeos

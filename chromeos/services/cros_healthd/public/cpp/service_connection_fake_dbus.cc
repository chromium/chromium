// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/cpp/fake_cros_healthd_client.h"

namespace chromeos {
namespace cros_healthd {

void InitFakeCrosHealthd() {
  FakeCrosHealthdClient::InitializeFake();
}

}  // namespace cros_healthd
}  // namespace chromeos

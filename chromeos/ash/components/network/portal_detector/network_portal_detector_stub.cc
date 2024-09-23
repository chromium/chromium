// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/portal_detector/network_portal_detector_stub.h"

namespace ash {

NetworkPortalDetectorStub::NetworkPortalDetectorStub() = default;

NetworkPortalDetectorStub::~NetworkPortalDetectorStub() = default;

bool NetworkPortalDetectorStub::IsEnabled() {
  return false;
}

void NetworkPortalDetectorStub::Enable() {}

}  // namespace ash

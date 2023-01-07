// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/exo/wayland/clients/test/client_version_test.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"

namespace {

using WaylandClientInterfaceTests = exo::WaylandClientTest;
using ClientVersionTest = exo::wayland::clients::ClientVersionTest;

// Test interface binding for version skew support.
TEST_F(WaylandClientInterfaceTests, InterfaceBinding) {
  const auto protocols = ClientVersionTest::Protocols();
  for (const auto& protocol : protocols) {
    LOG(INFO) << "Testing protocol: " << protocol;
    for (auto validity : {
             ClientVersionTest::VersionValidityType::INVALID_NULL,
             ClientVersionTest::VersionValidityType::VALID_SKEWED,
             ClientVersionTest::VersionValidityType::VALID_ADVERTISED,
             ClientVersionTest::VersionValidityType::INVALID_UNSUPPORTED,
         }) {
      ClientVersionTest::TestProtocol(protocol, validity);
    }
  }

  LOG(INFO) << "Successfully tested " << protocols.size()
            << " protocols (excluding zwp_linux_explicit_synchronization_v1)";
}
}  // namespace

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/logging.h"
#include "components/exo/wayland/clients/test/client_version_test.h"

int main(int argc, char* argv[]) {
  using ClientVersionTest = exo::wayland::clients::ClientVersionTest;
  const auto protocols = ClientVersionTest::Protocols();
  for (const auto& protocol : protocols) {
    LOG(INFO) << "Testing protocol: " << protocol;
    for (auto validity : {
             ClientVersionTest::VersionValidityType::INVALID_NULL,
             ClientVersionTest::VersionValidityType::VALID_SKEWED,
             ClientVersionTest::VersionValidityType::VALID_ADVERTISED,
             ClientVersionTest::VersionValidityType::INVALID_UNSUPPORTED,
         }) {
      ClientVersionTest client;
      client.TestProtocol(protocol, validity);
    }
  }

  LOG(INFO) << "Successfully tested " << protocols.size()
            << " protocols (excluding zwp_linux_explicit_synchronization_v1)";
  return 0;
}

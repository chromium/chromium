// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/logging.h"
#include "components/exo/wayland/clients/test/client_version_test.h"

int main(int argc, char* argv[]) {
  using ClientVersionTest = exo::wayland::clients::ClientVersionTest;
  ClientVersionTest client_list_protocols;
  client_list_protocols.TestProtocol(
      "", ClientVersionTest::VersionValidityType::INVALID_NULL);
  for (const auto& protocol : client_list_protocols.Protocols()) {
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

  LOG(INFO) << "Successfully tested "
            << client_list_protocols.Protocols().size()
            << " protocols (excluding zwp_linux_explicit_synchronization_v1)";
  return 0;
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_CLIENT_VERSION_TEST_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_CLIENT_VERSION_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "components/exo/wayland/clients/client_helper.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

struct Deleter;
}

/** A wayland client meant to test interface binding and version skew support.
 */
class ClientVersionTest {
 public:
  ClientVersionTest& operator=(const ClientVersionTest&) = delete;
  ClientVersionTest(const ClientVersionTest&) = delete;

  enum class VersionValidityType : int {
    // Will set the corresponding version to...
    INVALID_NULL = 0,  // ...0 which mean null protocol
    VALID_SKEWED = 1,  // ...(advertised_version - 1) if advertised is greater
                       // than 1, advertised_version otherwise
    VALID_ADVERTISED = 2,  // ...advertised_version
    INVALID_UNSUPPORTED =
        3,  // ...advertised_version + 1, greater than highest supported
  };

  // Test binding a given protocol with a given validity type. If the protocol
  // name is empty, then it will only initialize the list of protocol names.
  static void TestProtocol(const std::string& protocol,
                           VersionValidityType validity_type);

  ClientVersionTest() = default;
  ~ClientVersionTest() = default;

  static const std::vector<std::string> Protocols();
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_CLIENT_VERSION_TEST_H_

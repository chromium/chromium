// Copyright 2021 The Chromium Authors. All rights reserved.
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

  struct Globals {
    Globals();
    ~Globals();

    std::unique_ptr<wl_compositor> compositor;
    std::vector<std::string> protocols;
    std::string protocol_tested;
    VersionValidityType validity_type = VersionValidityType::VALID_ADVERTISED;
    // A dump all for all unused interface binding (test only), to avoid memory
    // leak.
    std::vector<std::unique_ptr<void, Deleter>> ptrs;
  };

  // Test binding a given protocol with a given validity type. If the protocol
  // name is empty, then it will only initialize the list of protocol names.
  bool TestProtocol(const std::string& protocol,
                    VersionValidityType validity_type);

  ClientVersionTest() = default;
  ~ClientVersionTest() = default;

  const std::vector<std::string>& Protocols() const;

 protected:
  Globals globals_;
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_CLIENT_VERSION_TEST_H_

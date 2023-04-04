// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wlcs/display_server.h"

namespace {

WlcsDisplayServer* create_server(int argc, char const** argv) {
  return new exo::wlcs::DisplayServer();
}

void destroy_server(WlcsDisplayServer* server) {
  delete static_cast<exo::wlcs::DisplayServer*>(server);
}

}  // namespace

// WLCS works by dynamically loading an integration module which is required to
// export the wlcs_server_integration symbol. See
// third_party/wlcs/src/include/disaply_server.h for details.
extern "C" __attribute__((visibility("default")))
WlcsServerIntegration const wlcs_server_integration{
    WLCS_SERVER_INTEGRATION_VERSION, &create_server, &destroy_server};

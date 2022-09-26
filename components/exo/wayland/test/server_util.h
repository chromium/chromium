// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_SERVER_UTIL_H_
#define COMPONENTS_EXO_WAYLAND_TEST_SERVER_UTIL_H_

#include <wayland-server.h>

#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/resource_key.h"

namespace exo::wayland::test::server_util {

// Iterates over all the clients of `server` to find the wl_resource matching
// `key`. Returns nullptr if not found.
wl_resource* LookUpResource(Server* server, const ResourceKey& key);

template <typename UserData>
UserData* GetUserDataForResource(Server* server, const ResourceKey& key) {
  wl_resource* resource = LookUpResource(server, key);
  if (!resource)
    return nullptr;

  return GetUserDataAs<UserData>(resource);
}

}  // namespace exo::wayland::test::server_util

#endif  // COMPONENTS_EXO_WAYLAND_TEST_SERVER_UTIL_H_

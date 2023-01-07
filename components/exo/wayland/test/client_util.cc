// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/client_util.h"

#include <wayland-client-core.h>

namespace exo::wayland::test::client_util {

ResourceKey GetResourceKey(void* resource) {
  wl_proxy* proxy = static_cast<wl_proxy*>(resource);
  return ResourceKey{.id = wl_proxy_get_id(proxy),
                     .class_name = wl_proxy_get_class(proxy)};
}

}  // namespace exo::wayland::test::client_util

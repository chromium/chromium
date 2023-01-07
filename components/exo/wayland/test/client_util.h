// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_CLIENT_UTIL_H_
#define COMPONENTS_EXO_WAYLAND_TEST_CLIENT_UTIL_H_

#include "components/exo/wayland/test/resource_key.h"

namespace exo::wayland::test::client_util {

// `resource` must be a Wayland client-side resource.
ResourceKey GetResourceKey(void* resource);

}  // namespace exo::wayland::test::client_util

#endif  // COMPONENTS_EXO_WAYLAND_TEST_CLIENT_UTIL_H_

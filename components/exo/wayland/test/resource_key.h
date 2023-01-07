// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_RESOURCE_KEY_H_
#define COMPONENTS_EXO_WAYLAND_TEST_RESOURCE_KEY_H_

#include <string>

namespace exo::wayland::test {

// Used to pass Wayland resource information between server and client.
struct ResourceKey {
  uint32_t id = 0;
  std::string class_name;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_RESOURCE_KEY_H_

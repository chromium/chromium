// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/weston_test.h"

namespace exo {
namespace wayland {
class Server;

struct WestonTest::WestonTestState {};

WestonTest::WestonTest(Server* server) {}

WestonTest::~WestonTest() = default;

}  // namespace wayland
}  // namespace exo

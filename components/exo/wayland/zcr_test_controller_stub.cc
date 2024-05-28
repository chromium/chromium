// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_test_controller.h"

namespace exo::wayland {

class Server;

TestController::TestController(Server* server) {}

TestController::~TestController() = default;

struct TestController::State {};

}  // namespace exo::wayland

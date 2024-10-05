// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/environment.h"

namespace {

// A dir on DUT to host wayland socket and arc-bridge sockets.
inline constexpr char kRunChrome[] = "/run/chrome";

}  // namespace

AshIntegrationTest::AshIntegrationTest() = default;

AshIntegrationTest::~AshIntegrationTest() = default;

void AshIntegrationTest::SetUpCommandLine(base::CommandLine* command_line) {
  InteractiveAshTest::SetUpCommandLine(command_line);

  // Enable the Wayland server.
  command_line->AppendSwitch(ash::switches::kAshEnableWaylandServer);

  // Set up XDG_RUNTIME_DIR for Wayland.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("XDG_RUNTIME_DIR", kRunChrome);
}

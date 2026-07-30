// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  ash::AshTestSuite test_suite(argc, argv);

  mojo::core::Configuration mojo_config;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestChildProcess)) {
    mojo_config.is_broker_process = true;
  }
  mojo::core::Init(mojo_config);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ash::AshTestSuite::Run, base::Unretained(&test_suite)));
}

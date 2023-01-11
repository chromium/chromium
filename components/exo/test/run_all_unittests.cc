// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  ash::AshTestSuite test_suite(argc, argv);

  mojo::core::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

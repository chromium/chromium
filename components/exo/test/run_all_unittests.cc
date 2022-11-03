// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#include "chromeos/ash/components/test/ash_test_suite.h"

#if !BUILDFLAG(IS_IOS)
#include "mojo/core/embedder/embedder.h"
#endif

int main(int argc, char** argv) {
  ash::AshTestSuite test_suite(argc, argv);

#if !BUILDFLAG(IS_IOS)
  mojo::core::Init();
#endif

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

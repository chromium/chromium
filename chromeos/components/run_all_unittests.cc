// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/components/chromeos_components_test_suite.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#error This test target only builds with linux-chromeos, not for real ChromeOS\
 devices. See comment in build/config/chromeos/args.gni.
#endif

int main(int argc, char** argv) {
  // Some unit tests make Mojo calls.
  mojo::core::Init();

  ChromeosComponentsTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&ChromeosComponentsTestSuite::Run,
                                              base::Unretained(&test_suite)));
}

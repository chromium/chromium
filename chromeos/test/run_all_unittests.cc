// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/chromeos_buildflags.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/test/ash_test_suite.h"
#else
#include "base/test/test_suite.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#error This test target only builds with linux-chromeos, not for real ChromeOS\
 devices. See comment in build/config/chromeos/args.gni.
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
using TestSuite = ash::AshTestSuite;
#else
using TestSuite = base::TestSuite;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

int main(int argc, char** argv) {
  // Some unit tests make Mojo calls.
  mojo::core::Init();

  TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&TestSuite::Run, base::Unretained(&test_suite)));
}

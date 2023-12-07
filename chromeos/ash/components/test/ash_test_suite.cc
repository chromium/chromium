// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/test/ash_test_suite.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/lottie/resource.h"

namespace ash {

AshTestSuite::AshTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

AshTestSuite::~AshTestSuite() = default;

void AshTestSuite::Initialize() {
  base::TestSuite::Initialize();

  testing::UnitTest::GetInstance()->listeners().Append(
      new ui::ProvideAXPlatformForTests());

  // Force software-gl. This is necessary for tests that trigger launching ash
  // in its own process
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kOverrideUseSoftwareGLForTests);

  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();

  // Force unittests to run using en-US so if we test against string output,
  // it'll pass regardless of the system language.
  base::i18n::SetICUDefaultLocale("en_US");

  ui::ResourceBundle::SetLottieParsingFunctions(
      &lottie::ParseLottieAsStillImage, &lottie::ParseLottieAsThemedStillImage);

  LoadTestResources();

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  env_ = aura::Env::CreateInstance();
}

void AshTestSuite::LoadTestResources() {
  // Load ash test resources and en-US strings; not 'common' (Chrome) resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_ASSETS, &path);
  base::FilePath ash_test_strings =
      path.Append(FILE_PATH_LITERAL("ash_test_strings.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ash_test_strings);

  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      path.AppendASCII("ash_test_resources_unscaled.pak"),
      ui::kScaleFactorNone);

  if (ui::IsScaleFactorSupported(ui::k100Percent)) {
    base::FilePath ash_test_resources_100 =
        path.AppendASCII("ash_test_resources_100_percent.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        ash_test_resources_100, ui::k100Percent);
  }
  if (ui::IsScaleFactorSupported(ui::k200Percent)) {
    base::FilePath ash_test_resources_200 =
        path.Append(FILE_PATH_LITERAL("ash_test_resources_200_percent.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        ash_test_resources_200, ui::k200Percent);
  }
}

void AshTestSuite::Shutdown() {
  env_.reset();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace ash

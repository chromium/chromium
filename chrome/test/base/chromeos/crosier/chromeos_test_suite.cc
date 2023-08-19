// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_test_suite.h"

#include "build/chromeos_buildflags.h"
#include "ui/events/test/event_generator.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ui_controls_ash.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check.h"
#include "base/files/file_util.h"
#include "chrome/common/chrome_paths_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

ChromeOSTestSuite::ChromeOSTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {}

ChromeOSTestSuite::~ChromeOSTestSuite() = default;

void ChromeOSTestSuite::Initialize() {
  content::ContentTestSuiteBase::Initialize();
  // chromeos_integration_tests must use functions in ui_controls.h.
  ui::test::EventGenerator::BanEventGenerator();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::test::EnableUIControlsAsh();
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitch(switches::kDisableMojoBroker);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // The lacros binary receives certain paths from ash very early in startup.
  // Simulate that behavior here. See chrome_paths_lacros.cc for details. The
  // specific path doesn't matter as long as it exists.
  CHECK(scoped_temp_dir_.CreateUniqueTempDir());
  base::FilePath temp_path = scoped_temp_dir_.GetPath();
  chrome::SetLacrosDefaultPaths(
      /*documents_dir=*/temp_path,
      /*downloads_dir=*/temp_path,
      /*drivefs=*/base::FilePath(),
      /*removable_media_dir=*/base::FilePath(),
      /*android_files_dir=*/base::FilePath(),
      /*linux_files_dir=*/base::FilePath(),
      /*ash_resources_dir=*/base::FilePath(),
      /*share_cache_dir=*/temp_path,
      /*preinstalled_web_app_config_dir=*/base::FilePath(),
      /*preinstalled_web_app_extra_config_dir=*/base::FilePath());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

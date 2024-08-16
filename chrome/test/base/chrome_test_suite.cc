// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/base/chrome_test_suite.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <stdio.h>
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/test/test_launcher.h"
#include "extensions/common/constants.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_paths.h"
#include "base/process/process_metrics.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check.h"
#include "base/files/file_util.h"
#include "chrome/common/chrome_paths_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

namespace {

bool IsCrosPythonProcess() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  char buf[80];
  int num_read = readlink(base::kProcSelfExe, buf, sizeof(buf) - 1);
  if (num_read == -1)
    return false;
  buf[num_read] = 0;
  const char kPythonPrefix[] = "/python";
  return !strncmp(strrchr(buf, '/'), kPythonPrefix, sizeof(kPythonPrefix) - 1);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

ChromeTestSuite::ChromeTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {
}

ChromeTestSuite::~ChromeTestSuite() = default;

void ChromeTestSuite::Initialize() {
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool autorelease_pool;
  chrome_browser_application_mac::RegisterBrowserCrApp();
#endif

  if (!browser_dir_.empty()) {
    base::PathService::Override(base::DIR_EXE, browser_dir_);
    base::PathService::Override(base::DIR_MODULE, browser_dir_);
  }

  // Disable external libraries load if we are under python process in
  // ChromeOS.  That means we are autotest and, if ASAN is used,
  // external libraries load crashes.
  if (!IsCrosPythonProcess())
    media::InitializeMediaLibrary();

  // Initialize after overriding paths as some content paths depend on correct
  // values for DIR_EXE and DIR_MODULE.
  content::ContentTestSuiteBase::Initialize();

  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
      ChromeMainDelegate::kNonWildcardDomainNonPortSchemes,
      ChromeMainDelegate::kNonWildcardDomainNonPortSchemesSize);

  // Desktop Identity Consistency (a.k.a. DICE) requires OAuth client to be
  // configured as it is needed for regular web sign-in flows to Google.
  // Ignore this requiement for unit and browser tests to make sure that the
  // DICE feature gets the right test coverage.
  AccountConsistencyModeManager::SetIgnoreMissingOAuthClientForTesting();

#if BUILDFLAG(IS_MAC)
  // Look in the framework bundle for resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::apple::SetOverrideFrameworkBundlePath(path);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The lacros binary receives certain paths from ash very early in startup.
  // Simulate that behavior here. See chrome_paths_lacros.cc for details. The
  // specific path doesn't matter as long as it exists.
  CHECK(scoped_temp_dir_.CreateUniqueTempDir());
  base::FilePath temp_path = scoped_temp_dir_.GetPath();
  chrome::SetLacrosDefaultPaths(
      /*documents_dir=*/temp_path,
      /*downloads_dir=*/temp_path,
      /*drivefs=*/base::FilePath(),
      /*onedrive=*/base::FilePath(),
      /*removable_media_dir=*/base::FilePath(),
      /*android_files_dir=*/base::FilePath(),
      /*linux_files_dir=*/base::FilePath(),
      /*ash_resources_dir=*/base::FilePath(),
      /*share_cache_dir=*/temp_path,
      /*preinstalled_web_app_config_dir=*/base::FilePath(),
      /*preinstalled_web_app_extra_config_dir=*/base::FilePath());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void ChromeTestSuite::Shutdown() {
#if BUILDFLAG(IS_MAC)
  base::apple::SetOverrideFrameworkBundlePath({});
#endif

  content::ContentTestSuiteBase::Shutdown();
}

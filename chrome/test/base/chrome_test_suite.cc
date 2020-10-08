// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_suite.h"

#if defined(OS_CHROMEOS)
#include <stdio.h>
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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

#if defined(OS_CHROMEOS)
#include "base/process/process_metrics.h"
#include "chromeos/constants/chromeos_paths.h"
#endif

#if defined(OS_MAC)
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

namespace {

bool IsCrosPythonProcess() {
#if defined(OS_CHROMEOS)
  char buf[80];
  int num_read = readlink(base::kProcSelfExe, buf, sizeof(buf) - 1);
  if (num_read == -1)
    return false;
  buf[num_read] = 0;
  const char kPythonPrefix[] = "/python";
  return !strncmp(strrchr(buf, '/'), kPythonPrefix, sizeof(kPythonPrefix) - 1);
#else
  return false;
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

ChromeTestSuite::ChromeTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {
}

ChromeTestSuite::~ChromeTestSuite() = default;

void ChromeTestSuite::Initialize() {
#if defined(OS_MAC)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
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

#if defined(OS_MAC)
  // Look in the framework bundle for resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::mac::SetOverrideFrameworkBundlePath(path);
#endif
}

void ChromeTestSuite::Shutdown() {
#if defined(OS_MAC)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif

  content::ContentTestSuiteBase::Shutdown();
}

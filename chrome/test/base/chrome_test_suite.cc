// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/base/chrome_test_suite.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <stdio.h>
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "extensions/common/constants.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_paths.h"
#include "base/process/process_metrics.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/common/chrome_switches.h"
#endif

namespace {

bool IsCrosPythonProcess() {
#if BUILDFLAG(IS_CHROMEOS)
  char buf[80];
  int num_read = readlink(base::kProcSelfExe, buf, sizeof(buf) - 1);
  if (num_read == -1)
    return false;
  buf[num_read] = 0;
  const char kPythonPrefix[] = "/python";
  return !strncmp(strrchr(buf, '/'), kPythonPrefix, sizeof(kPythonPrefix) - 1);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

ChromeTestSuite::ChromeTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {
}

ChromeTestSuite::~ChromeTestSuite() {
#if BUILDFLAG(IS_MAC)
  // In most (browser) tests, closing all Browser windows during test tear down
  // will trigger an applicationWillTerminate notification which causes
  // app_controller_mac to release its ScopedKeepAlive. However a select few
  // browser tests never create any Browser windows, thus never triggering this
  // logic. Call AllowApplicationToTerminate here explicitly to ensure that in
  // those tests the ScopedKeepAlive is released as well, allowing the test to
  // terminate.
  app_controller_mac::AllowApplicationToTerminate();
#endif
}

void ChromeTestSuite::Initialize() {
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool autorelease_pool;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDoNotCreateNSAppForTests)) {
    chrome_browser_application_mac::RegisterBrowserCrApp();
  }
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
  // Some features in //components/signin only work in builds with official
  // Chrome API keys. Ignore this requirement to get a better test coverage.
  signin::SetIgnoreNonOfficialApiKeys();

#if BUILDFLAG(IS_MAC)
  // Look in the framework bundle for resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::apple::SetOverrideFrameworkBundlePath(path);
#endif
}

void ChromeTestSuite::Shutdown() {
#if BUILDFLAG(IS_MAC)
  base::apple::SetOverrideFrameworkBundlePath({});
#endif

  content::ContentTestSuiteBase::Shutdown();
}

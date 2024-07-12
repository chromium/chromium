// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_launcher_utils.h"

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/os_crypt/sync/os_crypt_switches.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "content/public/common/content_switches.h"
#include "ui/display/display_switches.h"

#if defined(USE_AURA)
#include "ui/wm/core/wm_core_switches.h"
#endif

namespace test_launcher_utils {

void PrepareBrowserCommandLineForTests(base::CommandLine* command_line) {
  // Don't show the first run ui.
  command_line->AppendSwitch(switches::kNoFirstRun);

  // No default browser check, it would create an info-bar (if we are not the
  // default browser) that could conflicts with some tests expectations.
  command_line->AppendSwitch(switches::kNoDefaultBrowserCheck);

  // Enable info level logging to stderr by default so that we can see when bad
  // stuff happens, but honor the flags specified from the command line. Use the
  // default logging level (INFO) instead of explicitly passing
  // switches::kLoggingLevel. Passing the switch explicitly resulted in data
  // races in tests that start async operations (that use logging) prior to
  // initializing the browser: https://crbug.com/749066.
  if (!command_line->HasSwitch(switches::kEnableLogging))
    command_line->AppendSwitchASCII(switches::kEnableLogging, "stderr");

  // Don't install default apps.
  command_line->AppendSwitch(switches::kDisableDefaultApps);

#if defined(USE_AURA)
  // Disable window animations under Ash as the animations effect the
  // coordinates returned and result in flake.
  command_line->AppendSwitch(
      wm::switches::kWindowAnimationsDisabled);
#endif

#if BUILDFLAG(IS_LINUX)
  // Don't use the native password stores on Linux since they may
  // prompt for additional UI during tests and cause test failures or
  // timeouts.  Win, Mac and ChromeOS don't look at the kPasswordStore
  // switch.
  if (!command_line->HasSwitch(password_manager::kPasswordStore)) {
    command_line->AppendSwitchASCII(password_manager::kPasswordStore, "basic");
  }
#endif

#if BUILDFLAG(IS_MAC)
  // Use mock keychain on mac to prevent blocking permissions dialogs.
  command_line->AppendSwitch(os_crypt::switches::kUseMockKeychain);
#endif

  command_line->AppendSwitch(switches::kDisableComponentUpdate);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Changing the stack canary means we need to disable the stack guard on all
  // functions that appear as ancestors in the call stack of RunZygote(). This
  // is infeasible for tests, and changing the stack canary is unnecessary for
  // tests as it is a security mitigation.
  command_line->AppendSwitchASCII(switches::kChangeStackGuardOnFork,
                                  switches::kChangeStackGuardOnForkDisabled);
#endif
}

void PrepareBrowserCommandLineForBrowserTests(base::CommandLine* command_line,
                                              bool open_about_blank_on_launch) {
  // This is a Browser test.
  command_line->AppendSwitchASCII(switches::kTestType, "browser");

  if (open_about_blank_on_launch && command_line->GetArgs().empty())
    command_line->AppendArg(url::kAboutBlankURL);
}

bool CreateUserDataDir(base::ScopedTempDir* temp_dir) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  if (user_data_dir.empty()) {
    DCHECK(temp_dir);
    if (temp_dir->CreateUniqueTempDir() && temp_dir->IsValid()) {
      user_data_dir = temp_dir->GetPath();
    } else {
      LOG(ERROR) << "Could not create temporary user data directory \""
                 << temp_dir->GetPath().value() << "\".";
      return false;
    }
  }
  return OverrideUserDataDir(user_data_dir);
}

bool OverrideUserDataDir(const base::FilePath& user_data_dir) {
  bool success = true;

  // base::PathService::Override() is the best way to change the user data
  // directory. This matches what is done in ChromeMain().
  success = base::PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Make sure the cache directory is inside our clear profile. Otherwise
  // the cache may contain data from earlier tests that could break the
  // current test.
  //
  // Note: we use an environment variable here, because we have to pass the
  // value to the child process. This is the simplest way to do it.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  success = success && env->SetVar("XDG_CACHE_HOME", user_data_dir.value());

  // Also make sure that the machine policy directory is inside the clear
  // profile. Otherwise the machine's policies could affect tests.
  base::FilePath policy_files = user_data_dir.AppendASCII("policies");
  success = success &&
            base::PathService::Override(chrome::DIR_POLICY_FILES, policy_files);
#endif

  return success;
}

}  // namespace test_launcher_utils

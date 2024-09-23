// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_TEST_OVERRIDE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_TEST_OVERRIDE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/webapps/common/web_app_id.h"

#if BUILDFLAG(IS_WIN)
class ShellLinkItem;
#endif

namespace base {
class Environment;
}

namespace web_app {

class OsIntegrationTestOverrideImpl;

// This class is used to help test OS integration code and operations running on
// trybots. Among other complexities, trybots are often running multiple tests
// at the same times, so anything that operates in shared OS state could have
// side effects that this class attempts to solve. (For example, this class
// makes sure that on Mac, we 'install' the application to a temporary directory
// to avoid overwriting one from another test).
//
// The general rules for adding / using this are:
// - If the OS integration CAN be fully tested on a trybot, do so. The presence
//   of this class can allow customization of the integration if needed (e.g.
//   changing folders).
//   - If the information 'written' to the OS CAN be easily read back / verified
//     in a test, then no further work needed, and tests can do this.
//   - If the information 'written' to the OS CANNOT be easily read back /
//     verified in a test, then populate metadata in this object about the
//     final OS call for tests to check.
// - If the OS integration CANNOT be fully tested on a trybot (it doesn't work
//   or messes up the environment), then the presence of this object disables
//   the os integration, and information is populated about the final OS call in
//   this class.
// - Note: Using utilities like `RegistryOverrideManager` and
//   `ScopedPathOverride` are preferred to having the production code explicitly
//   check for the presence of this class & getting information from it.
//
// This class is used across multiple different sequenced task runners:
// - Created on the UI thread.
// - Accessed & sometimes modified by the shortcut task runner.
// - Accessed by the UI thread.
// It is up to the user to ensure thread safety of this class through
// ordering guarantees.
//
// This base class is built in the non-testing target, allowing production code
// to check for it's existence and iteract with it. The actual implementation is
// in `OsIntegrationTestOverrideImpl`, which testing-only and can thus use
// test-only features. That implementation has further methods for tests to
// check the OS integration state.
class OsIntegrationTestOverride
    : public base::RefCountedThreadSafe<OsIntegrationTestOverride> {
 public:
  static void CheckOsIntegrationAllowed();

  // This will return a nullptr in production code or tests that have not
  // created a `OsIntegrationTestOverrideImpl::BlockingRegistration` through
  // `OsIntegrationTestOverrideImpl::OverrideForTesting`.
  static scoped_refptr<OsIntegrationTestOverride> Get();

  OsIntegrationTestOverride(OsIntegrationTestOverride&&) = delete;
  OsIntegrationTestOverride(const OsIntegrationTestOverride&) = delete;

  // Safe downcasting to the testing implementation.
  virtual OsIntegrationTestOverrideImpl* AsOsIntegrationTestOverrideImpl();

#if BUILDFLAG(IS_WIN)
  // These should not be called from tests, these are automatically
  // called from production code in testing to set
  // up OS integration data for shortcuts menu registration and
  // unregistration.
  virtual void AddShortcutsMenuJumpListEntryForApp(
      const std::wstring& app_user_model_id,
      const std::vector<scoped_refptr<ShellLinkItem>>& shell_link_items) = 0;
  virtual void DeleteShortcutsMenuJumpListEntryForApp(
      const std::wstring& app_user_model_id) = 0;

  virtual base::FilePath desktop() = 0;
  virtual base::FilePath application_menu() = 0;
  virtual base::FilePath quick_launch() = 0;
  virtual base::FilePath startup() = 0;
#elif BUILDFLAG(IS_MAC)
  virtual bool IsChromeAppsValid() = 0;
  virtual base::FilePath chrome_apps_folder() = 0;
  virtual void EnableOrDisablePathOnLogin(const base::FilePath& file_path,
                                          bool enable_on_login) = 0;
#elif BUILDFLAG(IS_LINUX)
  virtual base::Environment* environment() = 0;
#endif

  // Creates a tuple of app_id to protocols and adds it to the vector
  // of registered protocols. There can be multiple entries for the same
  // app_id.
  virtual void RegisterProtocolSchemes(const webapps::AppId& app_id,
                                       std::vector<std::string> protocols) = 0;

 private:
  friend class base::RefCountedThreadSafe<OsIntegrationTestOverride>;
  friend class OsIntegrationTestOverrideImpl;
  friend class OsIntegrationTestOverrideBlockingRegistration;

  // Gets or creates a new OsIntegrationTestOverride globally. Creation is done
  // using the `creation_function`. Used by blocking registrations, and
  // increases the blocking registration count.
  static scoped_refptr<OsIntegrationTestOverride>
  GetOrCreateForBlockingRegistration(
      base::FunctionRef<scoped_refptr<OsIntegrationTestOverride>()>
          creation_function);

  // Decreases the blocking registration in the global struct. If there are no
  // more registrations, the global value is reset and returns `true`.
  static bool DecreaseBlockingRegistrationCountMaybeReset();

  OsIntegrationTestOverride();
  virtual ~OsIntegrationTestOverride() = 0;
};

}  // namespace web_app

#define CHECK_OS_INTEGRATION_ALLOWED() \
  OsIntegrationTestOverride::CheckOsIntegrationAllowed()

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_TEST_OVERRIDE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_ARC_MIXIN_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_ARC_MIXIN_H_

#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chromeos/crosier/adb_helper.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace aura {
class Window;
}

namespace base {
class FilePath;
}

class ChromeOSIntegrationLoginMixin;

// ChromeOSIntegrationArcMixin provides ARC support to ChromeOS integration
// test.
class ChromeOSIntegrationArcMixin : public InProcessBrowserTestMixin {
 public:
  enum class Mode {
    // ARC is not enabled.
    kNone,

    // ARC is enabled with no play store.
    kEnabled,

    // Full ARC support. ARC is enabled and could perform play store optin. This
    // requires Gaia identity.
    // TODO(b/306225047): Implement this.
    kSupported,
  };

  ChromeOSIntegrationArcMixin(InProcessBrowserTestMixinHost* host,
                              const ChromeOSIntegrationLoginMixin& login_mixin);
  ChromeOSIntegrationArcMixin(const ChromeOSIntegrationArcMixin&) = delete;
  ChromeOSIntegrationArcMixin& operator=(const ChromeOSIntegrationArcMixin&) =
      delete;
  ~ChromeOSIntegrationArcMixin() override;

  // Sets the ARC mode. Must be called before SetUp.
  void SetMode(Mode mode);

  // Waits for ARC boot_completed and connect adb.
  void WaitForBootAndConnectAdb();

  // Installs the apk at the given path on the DUT.
  bool InstallApk(const base::FilePath& apk_path);

  // Launches the give activity and wait for its window to be activated.
  aura::Window* LaunchAndWaitForWindow(const std::string& package,
                                       const std::string& activity);

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  const ChromeOSIntegrationLoginMixin& login_mixin_;

  bool setup_called_ = false;
  Mode mode_ = Mode::kNone;
  std::optional<base::test::ScopedFeatureList> scoped_feature_list_;

  AdbHelper adb_helper_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_ARC_MIXIN_H_

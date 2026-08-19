// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser/isolated_browser_support.h"

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

constexpr std::string_view KTestGroupName = "Enabled_20260623";

class IsolatedBrowserSupportBrowserTest : public InProcessBrowserTest {
 public:
  IsolatedBrowserSupportBrowserTest() {
    // `dummy_param` is needed to fully initialize the field trial.
    feature_list_.InitFromCommandLine(
        base::StrCat({features::kIsolatedProcess.name, "<TestIsolationStudy.",
                      KTestGroupName, ":dummy_param/1"}),
        /*disable_features=*/std::string());
  }

  ~IsolatedBrowserSupportBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);
    scoped_install_details_ =
        std::make_unique<install_static::ScopedInstallDetails>(
            /*system_level=*/true);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  registry_util::RegistryOverrideManager registry_override_;
  std::unique_ptr<install_static::ScopedInstallDetails> scoped_install_details_;
};

IN_PROC_BROWSER_TEST_F(IsolatedBrowserSupportBrowserTest,
                       IsolationStateEnabled) {
  // On startup, the feature state is read and this sets isolation to enabled.
  // This is run on a delayed task on the UI sequence so the test must wait for
  // this to complete.
  ASSERT_TRUE(
      base::test::RunUntil([]() { return chrome::IsIsolationEnabled(); }));

  // Verify that the pref was also correctly updated.
  EXPECT_EQ(KTestGroupName, g_browser_process->local_state()->GetString(
                                prefs::kPreviousIsolationState));
}

}  // namespace chrome

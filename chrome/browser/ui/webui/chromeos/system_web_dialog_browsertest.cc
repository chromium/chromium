// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/web_preferences.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/aura/client/aura_constants.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";

class MockSystemWebDialog : public SystemWebDialogDelegate {
 public:
  explicit MockSystemWebDialog(const char* id = nullptr)
      : SystemWebDialogDelegate(GURL(chrome::kChromeUIIntenetConfigDialogURL),
                                base::string16()) {
    if (id)
      id_ = std::string(id);
  }
  ~MockSystemWebDialog() override = default;

  const std::string& Id() override { return id_; }
  std::string GetDialogArgs() const override { return std::string(); }

 private:
  std::string id_;
  DISALLOW_COPY_AND_ASSIGN(MockSystemWebDialog);
};

}  // namespace

class SystemWebDialogLoginTest : public LoginManagerTest {
 public:
  SystemWebDialogLoginTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
  ~SystemWebDialogLoginTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemWebDialogLoginTest);
};

// Verifies that system dialogs are modal before login (e.g. during OOBE).
IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, ModalTest) {
  auto* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_TRUE(ash::ShellTestApi().IsSystemModalWindowOpen());
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, PRE_NonModalTest) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  StartupUtils::MarkOobeCompleted();
}

// Verifies that system dialogs are not modal and always-on-top after login.
IN_PROC_BROWSER_TEST_F(SystemWebDialogLoginTest, NonModalTest) {
  LoginUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  auto* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();
  EXPECT_FALSE(ash::ShellTestApi().IsSystemModalWindowOpen());
  aura::Window* window_to_test = dialog->dialog_window();
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            window_to_test->GetProperty(aura::client::kZOrderingKey));
}

using SystemWebDialogTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemWebDialogTest, InstanceTest) {
  const char* kDialogId = "dialog_id";
  SystemWebDialogDelegate* dialog = new MockSystemWebDialog(kDialogId);
  dialog->ShowSystemDialog();
  SystemWebDialogDelegate* found_dialog =
      SystemWebDialogDelegate::FindInstance(kDialogId);
  EXPECT_EQ(dialog, found_dialog);
  // Closing (deleting) the dialog causes a crash in WebDialogView when the main
  // loop is run. TODO(stevenjb): Investigate, fix, and test closing the dialog.
  // https://crbug.com/855344.
}

class SystemWebDialogTestWithSplitSettings : public SystemWebDialogTest {
 public:
  SystemWebDialogTestWithSplitSettings() {
    feature_list_.InitAndEnableFeature(chromeos::features::kSplitSettings);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SystemWebDialogTestWithSplitSettings, FontSize) {
  const content::WebPreferences kDefaultPrefs;
  const int kDefaultFontSize = kDefaultPrefs.default_font_size;
  const int kDefaultFixedFontSize = kDefaultPrefs.default_fixed_font_size;

  // Set the browser font sizes to non-default values.
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  profile_prefs->SetInteger(prefs::kWebKitDefaultFontSize,
                            kDefaultFontSize + 2);
  profile_prefs->SetInteger(prefs::kWebKitDefaultFixedFontSize,
                            kDefaultFixedFontSize + 1);

  // Open a system dialog.
  MockSystemWebDialog* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();

  // Dialog font sizes are still the default values.
  content::WebPreferences dialog_prefs = dialog->GetWebUIForTest()
                                             ->GetWebContents()
                                             ->GetRenderViewHost()
                                             ->GetWebkitPreferences();
  EXPECT_EQ(kDefaultFontSize, dialog_prefs.default_font_size);
  EXPECT_EQ(kDefaultFixedFontSize, dialog_prefs.default_fixed_font_size);
}

IN_PROC_BROWSER_TEST_F(SystemWebDialogTestWithSplitSettings, PageZoom) {
  // Set the default browser page zoom to 150%.
  double level = blink::PageZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  // Open a system dialog.
  MockSystemWebDialog* dialog = new MockSystemWebDialog();
  dialog->ShowSystemDialog();

  // Dialog page zoom is still 100%.
  auto* web_contents = dialog->GetWebUIForTest()->GetWebContents();
  double dialog_level = content::HostZoomMap::GetZoomLevel(web_contents);
  EXPECT_TRUE(blink::PageZoomValuesEqual(dialog_level,
                                         blink::PageZoomFactorToZoomLevel(1.0)))
      << dialog_level;
}

}  // namespace chromeos

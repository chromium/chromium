// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/create_shortcut_confirmation_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"

struct Params {
  bool tab_strip_enabled;
};

class CreateShortcutConfirmationViewBrowserTest
    : public DialogBrowserTest,
      public ::testing::WithParamInterface<Params> {
 public:
  CreateShortcutConfirmationViewBrowserTest() = default;
  CreateShortcutConfirmationViewBrowserTest(
      const CreateShortcutConfirmationViewBrowserTest&) = delete;
  CreateShortcutConfirmationViewBrowserTest& operator=(
      const CreateShortcutConfirmationViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));
    app_info->title = u"Test app";

    auto callback = [](bool result,
                       std::unique_ptr<web_app::WebAppInstallInfo>) {};

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
        webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
            ->RegisterCurrentInstallForWebContents(
                webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

    web_app::ShowCreateShortcutDialog(web_contents, std::move(app_info),
                                     std::move(install_tracker),
                                     base::BindLambdaForTesting(callback));
  }

  void SetUp() override {
    base::flat_map<base::test::FeatureRef, bool> features;

    features.insert(
        {blink::features::kDesktopPWAsTabStrip, GetParam().tab_strip_enabled});
    features.insert(
        {features::kDesktopPWAsTabStripSettings, GetParam().tab_strip_enabled});

    feature_list.InitWithFeatureStates(features);
    DialogBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_P(CreateShortcutConfirmationViewBrowserTest,
                       ShowCreateShortcutDialog) {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com"));
  app_info->title = u"Test app";

  web_app::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                               /*auto_open_in_window=*/true);
  bool is_accepted = false;
  std::unique_ptr<web_app::WebAppInstallInfo> install_info;
  auto callback = [&is_accepted, &install_info](
                      bool result,
                      std::unique_ptr<web_app::WebAppInstallInfo> info) {
    is_accepted = result;
    install_info = std::move(info);
  };

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
          ->RegisterCurrentInstallForWebContents(
              webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

  web_app::ShowCreateShortcutDialog(web_contents, std::move(app_info),
                                   std::move(install_tracker),
                                   base::BindLambdaForTesting(callback));
  EXPECT_TRUE(is_accepted);

  EXPECT_EQ(install_info->user_display_mode,
            web_app::mojom::UserDisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_P(CreateShortcutConfirmationViewBrowserTest,
                       VerifyCreateShortcutDialogContents) {
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com"));
  app_info->title = u"Test app";

  web_app::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/false,
                                               /*auto_open_in_window=*/false);
  base::test::TestFuture<bool, std::unique_ptr<web_app::WebAppInstallInfo>>
      install_result;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
          ->RegisterCurrentInstallForWebContents(
              webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

  web_app::ShowCreateShortcutDialog(web_contents, std::move(app_info),
                                   std::move(install_tracker),
                                   install_result.GetCallback());

  CreateShortcutConfirmationView* dialog =
      CreateShortcutConfirmationView::GetDialogForTesting();

  ASSERT_TRUE(dialog);
  EXPECT_TRUE(dialog->GetVisible());

  dialog->Accept();

  EXPECT_TRUE(install_result.Get<bool /*is_accepted*/>());

  EXPECT_EQ(install_result.Get<std::unique_ptr<web_app::WebAppInstallInfo>>()
                ->user_display_mode,
            web_app::mojom::UserDisplayMode::kBrowser);
}

IN_PROC_BROWSER_TEST_P(CreateShortcutConfirmationViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(CreateShortcutConfirmationViewBrowserTest,
                       NormalizeTitles) {
  web_app::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                               /*auto_open_in_window=*/true);

  struct TestCases {
    std::u16string input;
    std::u16string expected_result;
  } test_cases[] = {
      {u"App Title", u"App Title"},
      {u"http://example.com", u"example.com"},
      {u"https://example.com", u"example.com"},
  };

  for (const TestCases& test_case : test_cases) {
    auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));
    app_info->title = test_case.input;

    bool is_accepted = false;
    std::u16string title;
    auto callback = [&is_accepted, &title](
                        bool result,
                        std::unique_ptr<web_app::WebAppInstallInfo> info) {
      is_accepted = result;
      title = info->title;
    };

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
        webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
            ->RegisterCurrentInstallForWebContents(
                webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

    web_app::ShowCreateShortcutDialog(web_contents, std::move(app_info),
                                     std::move(install_tracker),
                                     base::BindLambdaForTesting(callback));
    EXPECT_TRUE(is_accepted) << test_case.input;
    EXPECT_EQ(test_case.expected_result, title) << test_case.input;
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CreateShortcutConfirmationViewBrowserTest,
                         ::testing::Values(Params{false}, Params{true}));

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/deprecated_apps_dialog_view.h"

#include <set>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {
constexpr char mock_app_manifest1[] =
    "{"
    "  \"name\": \"Test App1\","
    "  \"version\": \"1\","
    "  \"manifest_version\": 2,"
    "  \"app\": {"
    "    \"launch\": {"
    "      \"web_url\": \"%s\""
    "    },"
    "    \"urls\": [\"*://app1.com/\"]"
    "  }"
    "}";
constexpr char mock_app_manifest2[] =
    "{"
    "  \"name\": \"Test App2\","
    "  \"version\": \"1\","
    "  \"manifest_version\": 2,"
    "  \"app\": {"
    "    \"launch\": {"
    "      \"web_url\": \"%s\""
    "    },"
    "    \"urls\": [\"*://app2.com/\"]"
    "  }"
    "}";
constexpr char mock_url1[] = "https://www.app1.com/index.html";
constexpr char mock_url2[] = "https://www.app2.com/index.html";
}  // namespace

class DeprecatedAppDialogWidgetObserver : public views::WidgetObserver {
 public:
  explicit DeprecatedAppDialogWidgetObserver(views::Widget* widget)
      : widget_(widget) {
    DCHECK(widget_);
    widget_->AddObserver(this);
  }

  ~DeprecatedAppDialogWidgetObserver() override {}

  void Wait() { run_loop_.Run(); }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    run_loop_.Quit();
  }

 private:
  raw_ptr<views::Widget> widget_;
  base::RunLoop run_loop_;
};

class DeprecatedAppsDialogViewBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  DeprecatedAppsDialogViewBrowserTest() {
    feature_list_.InitWithFeatures({features::kChromeAppsDeprecation}, {});
  }

  DeprecatedAppsDialogViewBrowserTest(
      const DeprecatedAppsDialogViewBrowserTest&) = delete;
  DeprecatedAppsDialogViewBrowserTest& operator=(
      const DeprecatedAppsDialogViewBrowserTest&) = delete;

  bool IsDialogShown() {
    if (test_dialog_view_)
      return true;
    return false;
  }

  void WaitForDialogToBeDestroyed() {
    if (!test_dialog_view_)
      return;

    DeprecatedAppDialogWidgetObserver dialog_observer(
        test_dialog_view_.get()->GetWidget());
    dialog_observer.Wait();
    test_dialog_view_ = nullptr;
  }

  // A return value of -1 means that the table has not been initialized
  // for the dialog.
  int GetRowCountForDialog() {
    views::TableView* table_for_dialog = GetTableViewForTesting();
    if (table_for_dialog) {
      return table_for_dialog->GetRowCount();
    }
    return -1;
  }

  views::TableView* GetTableViewForTesting() {
    if (IsDialogShown()) {
      return static_cast<views::TableView*>(test_dialog_view_->GetViewByID(
          DeprecatedAppsDialogView::DEPRECATED_APPS_TABLE));
    }
    return nullptr;
  }

  extensions::ExtensionId InstallExtensionForTesting(const char* app_manifest,
                                                     const char* url) {
    extensions::TestExtensionDir test_app_dir;
    test_app_dir.WriteManifest(
        base::StringPrintf(app_manifest, GURL(url).spec().c_str()));
    const extensions::Extension* app = InstallExtensionWithSourceAndFlags(
        test_app_dir.UnpackedPath(), /*expected_change=*/1,
        extensions::mojom::ManifestLocation::kInternal,
        extensions::Extension::NO_FLAGS);
    DCHECK(app);
    deprecated_app_ids_for_testing_.insert(app->id());
    return app->id();
  }

 protected:
  std::set<extensions::ExtensionId> deprecated_app_ids_for_testing_;
  base::WeakPtr<DeprecatedAppsDialogView> test_dialog_view_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeprecatedAppsDialogViewBrowserTest,
                       VerifyTableModelForSingleApp) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  InstallExtensionForTesting(mock_app_manifest1, mock_url1);
  test_dialog_view_ = DeprecatedAppsDialogView::CreateAndShowDialog(
                          deprecated_app_ids_for_testing_, web_contents)
                          ->AsWeakPtr();

  EXPECT_TRUE(IsDialogShown());
  EXPECT_EQ(static_cast<int>(deprecated_app_ids_for_testing_.size()),
            GetRowCountForDialog());
}

IN_PROC_BROWSER_TEST_F(DeprecatedAppsDialogViewBrowserTest,
                       VerifyTableModelForMultipleApps) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  InstallExtensionForTesting(mock_app_manifest1, mock_url1);
  InstallExtensionForTesting(mock_app_manifest2, mock_url2);
  test_dialog_view_ = DeprecatedAppsDialogView::CreateAndShowDialog(
                          deprecated_app_ids_for_testing_, web_contents)
                          ->AsWeakPtr();

  EXPECT_TRUE(IsDialogShown());
  EXPECT_EQ(static_cast<int>(deprecated_app_ids_for_testing_.size()),
            GetRowCountForDialog());
}

IN_PROC_BROWSER_TEST_F(DeprecatedAppsDialogViewBrowserTest,
                       AcceptDialogAndVerify) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  extensions::ExtensionId test_id(
      InstallExtensionForTesting(mock_app_manifest1, mock_url1));
  test_dialog_view_ = DeprecatedAppsDialogView::CreateAndShowDialog(
                          deprecated_app_ids_for_testing_, web_contents)
                          ->AsWeakPtr();

  // Verify dialog is shown.
  ASSERT_TRUE(IsDialogShown());
  EXPECT_EQ(static_cast<int>(deprecated_app_ids_for_testing_.size()),
            GetRowCountForDialog());

  // Verify dialog is closed on acceptance.
  ASSERT_TRUE(test_dialog_view_->Accept());
  WaitForDialogToBeDestroyed();
  ASSERT_FALSE(IsDialogShown());

  // Verify successful uninstallation of app.
  ASSERT_TRUE(
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext())
          ->GetInstalledExtension(test_id) == nullptr);
}

IN_PROC_BROWSER_TEST_F(DeprecatedAppsDialogViewBrowserTest,
                       CloseDialogAndVerify) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  InstallExtensionForTesting(mock_app_manifest1, mock_url1);
  test_dialog_view_ = DeprecatedAppsDialogView::CreateAndShowDialog(
                          deprecated_app_ids_for_testing_, web_contents)
                          ->AsWeakPtr();

  // Verify dialog is shown.
  ASSERT_TRUE(IsDialogShown());
  EXPECT_EQ(static_cast<int>(deprecated_app_ids_for_testing_.size()),
            GetRowCountForDialog());

  // Verify dialog is closed on cancellation
  ASSERT_TRUE(test_dialog_view_->Cancel());
  WaitForDialogToBeDestroyed();
  ASSERT_FALSE(IsDialogShown());
}

IN_PROC_BROWSER_TEST_F(DeprecatedAppsDialogViewBrowserTest,
                       DialogDoesNotLoadOnNavigationToChromeApps) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL)));
  ASSERT_FALSE(IsDialogShown());
}

IN_PROC_BROWSER_TEST_F(DeprecatedAppsDialogViewBrowserTest,
                       DeprecatedAppsDialogShownFromLinkClick) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  InstallExtensionForTesting(mock_app_manifest1, mock_url1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL)));
  auto waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{}, "DeprecatedAppsDialogView");
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      u"document.getElementById('deprecated-apps-link').click()",
      base::NullCallback());
  EXPECT_NE(waiter.WaitIfNeededAndGet(), nullptr);
}

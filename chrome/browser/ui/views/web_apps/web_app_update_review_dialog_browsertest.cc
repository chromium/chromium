// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_update_review_dialog.h"

#include <optional>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/web_app_update_identity_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app.to_value.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/test/sk_gmock_support.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/root_view.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using base::BucketsAre;

constexpr char kUpdateDialogLoadingTimeHistogram[] =
    "WebApp.UpdateReviewDialog.TriggerToShowTime";
constexpr char kAppUpdateDialogResultHistogram[] =
    "WebApp.PredictableUpdateDialog.Result";

// Clicks the ignore button on the update review dialog, being passed in as a
// widget.
void ClickIgnoreButtonOnDialog(views::Widget* dialog_widget) {
  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_widget);
  views::ElementTrackerViews* tracker_views =
      views::ElementTrackerViews::GetInstance();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog_widget);
  views::Button* button = tracker_views->GetFirstMatchingViewAs<views::Button>(
      kWebAppUpdateReviewIgnoreButton, context);
  ASSERT_NE(nullptr, button);
  views::test::ButtonTestApi(button).NotifyClick(ui::test::TestEvent());
  destroyed_waiter.Wait();
}

class WebAppUpdateReviewDialog : public DialogBrowserTest {
 public:
  const GURL kTestUrl = GURL("http://www.test.com");
  WebAppUpdateReviewDialog() = default;
  WebAppUpdateReviewDialog(const WebAppUpdateReviewDialog&) = delete;
  WebAppUpdateReviewDialog& operator=(const WebAppUpdateReviewDialog&) = delete;

  void SetUpOnMainThread() override {
    old_icon_ = ::shortcuts::GenerateBitmap(kIconSizeForUpdateDialog, u"A");
    new_icon_ = ::shortcuts::GenerateBitmap(kIconSizeForUpdateDialog, u"D");

    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kTestUrl);
    web_app_info->title = u"Abc";
    web_app_info->icon_bitmaps.any[kIconSizeForUpdateDialog] = old_icon_;
    web_app_info->trusted_icon_bitmaps.any[kIconSizeForUpdateDialog] =
        old_icon_;
    app_id_ = web_app::test::InstallWebApp(profile(), std::move(web_app_info));
    update_.old_title = u"Abc";
    update_.old_icon = gfx::Image::CreateFrom1xBitmap(old_icon_);
    update_.old_start_url = kTestUrl;
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Modify the update configuration based on the test name.
    if (base::Contains(name, "NameChange")) {
      update_.new_title =
          u"Definitely a longer title that is really really really really "
          u"long.";
    }
    if (base::Contains(name, "IconChange")) {
      update_.new_icon = gfx::Image::CreateFrom1xBitmap(new_icon_);
    }
    if (base::Contains(name, "UrlChange")) {
      update_.new_start_url = GURL("http://other.test.com");
    }

    web_app::ShowWebAppReviewUpdateDialog(app_id_, update_, browser(),
                                          base::TimeTicks::Now(),
                                          dialog_result_.GetCallback());
  }

  Profile* profile() { return browser()->profile(); }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi()) {
      return false;
    }
    bool is_showing =
        browser()->GetBrowserView().GetProperty(kIsPwaUpdateDialogShowingKey);
    EXPECT_TRUE(is_showing);
    return is_showing;
  }

 protected:
  SkBitmap old_icon_;
  SkBitmap new_icon_;
  WebAppIdentityUpdate update_;
  std::string app_id_;
  base::test::TestFuture<WebAppIdentityUpdateResult> dialog_result_;
  base::HistogramTester tester_;
};

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, InvokeUi_NameChange) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  ShowAndVerifyUi();
  histogram_tester.ExpectTotalCount(kUpdateDialogLoadingTimeHistogram, 1);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("PredictableAppUpdateDialogShown"));
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_NameChange_IconChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_NameChange_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_NameChange_IconChange_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, InvokeUi_IconChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_IconChange_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, InvokeUi_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       CloseUpdateReviewDialogOnUninstall) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  ShowUi("NameChange");
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);
  ASSERT_FALSE(dialog_widget->IsClosed());

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::RunLoop run_loop;
  observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget == dialog_widget) {
          run_loop.Quit();
        }
      }));
  // Uninstalling the app will close the update dialog
  web_app::test::UninstallWebApp(browser()->profile(), app_id_);
  run_loop.Run();

  EXPECT_FALSE(
      browser()->GetBrowserView().GetProperty(kIsPwaUpdateDialogShowingKey));
  EXPECT_EQ(dialog_result_.Get(),
            WebAppIdentityUpdateResult::kAppUninstalledDuringDialog);
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       CloseUpdateReviewDialogOnIgnore) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  ShowUi("NameChange");
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);
  ASSERT_FALSE(dialog_widget->IsClosed());

  // Verify dialog is closed, and the ignore result is obtained.
  ClickIgnoreButtonOnDialog(dialog_widget);
  EXPECT_FALSE(
      browser()->GetBrowserView().GetProperty(kIsPwaUpdateDialogShowingKey));
  EXPECT_EQ(dialog_result_.Get(), WebAppIdentityUpdateResult::kIgnore);
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, ShowWhileAlreadyShowing) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  ShowUi("NameChange");
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);
  ASSERT_FALSE(dialog_widget->IsClosed());

  base::test::TestFuture<WebAppIdentityUpdateResult> update_result;
  web_app::ShowWebAppReviewUpdateDialog(app_id_, update_, browser(),
                                        base::TimeTicks::Now(),
                                        update_result.GetCallback());
  EXPECT_TRUE(update_result.Wait());
  EXPECT_EQ(update_result.Get(), WebAppIdentityUpdateResult::kUnexpectedError);
  dialog_widget->Close();
}

struct DialogNames {
  std::u16string from_name;
  std::u16string to_name;
};

// Move operations only for simplicity.
struct DialogIcons {
  DialogIcons() = default;
  ~DialogIcons() = default;
  DialogIcons(DialogIcons&&) = default;
  DialogIcons& operator=(DialogIcons&&) = default;

  SkBitmap from_icon_bitmap;
  SkBitmap to_icon_bitmap;
};

// Test class that verifies the complete end to end flow of the dialog showing
// up after being clicked from the app window's 3-dot menu.
class WebAppUpdateDialogBrowserTests : public WebAppBrowserTestBase {
 public:
  WebAppUpdateDialogBrowserTests() = default;
  WebAppUpdateDialogBrowserTests(const WebAppUpdateDialogBrowserTests&) =
      delete;
  WebAppUpdateDialogBrowserTests& operator=(
      const WebAppUpdateDialogBrowserTests&) = delete;

  Profile* profile() { return browser()->profile(); }

  const webapps::AppId InstallAppAndTriggerAppUpdateDialog() {
    // Install the app and trigger a navigation.
    const GURL app_url =
        https_server()->GetURL("/web_apps/updating/index.html");
    const webapps::AppId app_id =
        InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);
    Browser* app_browser = LaunchWebAppBrowser(app_id);
    EXPECT_NE(app_browser, nullptr);
    // Ensure that the app browser is visible before proceeding. This ensures
    // that all PWA launching processes have finished.
    views::test::WidgetVisibleWaiter(app_browser->GetBrowserView().GetWidget())
        .Wait();
    // TODO(crbug.com/442643377): Delete this wait after the update runs for
    // every navigation.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();

    // Trigger an update, verify pending update info stored.
    const GURL update_url =
        https_server()->GetURL("/web_apps/updating/new_icon_page_masking.html");
    {
      UpdateAwaiter awaiter(provider().install_manager());
      EXPECT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
      awaiter.AwaitUpdate();
      provider().command_manager().AwaitAllCommandsCompleteForTesting();
    }

    // Mimic the "click" on the menu entry for app updating, verify the update
    // dialog shows up.
    WebAppMenuModel model(/*provider=*/nullptr, app_browser);
    model.Init();
    model.ExecuteCommand(IDC_WEB_APP_UPGRADE_DIALOG, /*event_flags=*/0);
    return app_id;
  }

 protected:
  // Returns the collection of views in `dialog_widget` corresponding to
  // element identifier `id`.
  std::vector<views::View*> GetViewsFromDialog(views::Widget* dialog_widget,
                                               ui::ElementIdentifier id) {
    views::ElementTrackerViews* tracker_views =
        views::ElementTrackerViews::GetInstance();
    ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(dialog_widget);
    return tracker_views->GetAllMatchingViews(id, context);
  }

  SkBitmap LoadExpectedBitmapFromDisk(const base::FilePath& file_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath image_path =
        base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
            .Append(file_path);
    std::optional<std::vector<uint8_t>> png_data =
        base::ReadFileToBytes(image_path);
    CHECK(png_data.has_value());
    SkBitmap read_bitmap =
        gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(*png_data))
            .AsBitmap();
    CHECK_EQ(read_bitmap.width(), web_app::kIconSizeForUpdateDialog);
    return read_bitmap;
  }

  // Icons will be masked on Mac and ChromeOS.
  const base::FilePath GetToIconFilePath() {
#if BUILDFLAG(IS_MAC)
    return base::FilePath(FILE_PATH_LITERAL(
        "chrome/test/data/web_apps/updating/green-masked-mac-96.png"));
#elif BUILDFLAG(IS_CHROMEOS)
    return base::FilePath(FILE_PATH_LITERAL(
        "chrome/test/data/web_apps/updating/green-masked-chromeos-96.png"));
#else
    return base::FilePath(
        FILE_PATH_LITERAL("chrome/test/data/web_apps/updating/green-96.png"));
#endif
  }

  const base::FilePath GetFromIconFilePath() {
    return base::FilePath(
        FILE_PATH_LITERAL("chrome/test/data/web_apps/updating/from_icon.png"));
  }

  // Returns the name of the apps on the dialog, and verifies that there should
  // only be 2 names on the dialog.
  DialogNames GetNamesFromDialog(views::Widget* dialog_widget) {
    DialogNames names;
    std::vector<views::View*> name_labels = GetViewsFromDialog(
        dialog_widget, web_app::WebAppUpdateIdentityView::kNameLabelId);
    EXPECT_EQ(2u, name_labels.size());

    views::Label* from_label = views::AsViewClass<views::Label>(name_labels[0]);
    views::Label* to_label = views::AsViewClass<views::Label>(name_labels[1]);
    names.from_name = from_label->GetText();
    names.to_name = to_label->GetText();
    return names;
  }

  // Returns the icons of the apps on the dialog, and verifies that there should
  // only be 2 icons on the dialog.
  DialogIcons GetIconsFromDialog(views::Widget* dialog_widget) {
    DialogIcons icons;
    std::vector<views::View*> icon_labels = GetViewsFromDialog(
        dialog_widget, web_app::WebAppUpdateIdentityView::kIconLabelId);
    EXPECT_EQ(2u, icon_labels.size());

    views::ImageView* from_icon =
        views::AsViewClass<views::ImageView>(icon_labels[0]);
    views::ImageView* to_icon =
        views::AsViewClass<views::ImageView>(icon_labels[1]);
    EXPECT_FALSE(from_icon->GetImageModel().IsEmpty());
    EXPECT_FALSE(to_icon->GetImageModel().IsEmpty());
    icons.from_icon_bitmap = *from_icon->GetImage().bitmap();
    icons.to_icon_bitmap = *to_icon->GetImage().bitmap();
    return icons;
  }

  base::HistogramTester tester_;
  base::test::ScopedFeatureList feature_list_{
      features::kWebAppPredictableAppUpdating};
};

IN_PROC_BROWSER_TEST_F(WebAppUpdateDialogBrowserTests,
                       MenuClickTriggersDialogVerifyIdentity) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  const webapps::AppId& app_id = InstallAppAndTriggerAppUpdateDialog();
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, dialog_widget);

  // At this point, the update has not been triggered yet.
  const WebApp* old_web_app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(old_web_app->pending_update_info().has_value());
  std::u16string old_name = base::UTF8ToUTF16(old_web_app->untranslated_name());

  // Verify names on the dialog.
  DialogNames names = GetNamesFromDialog(dialog_widget);
  EXPECT_EQ(old_name, names.from_name);
  EXPECT_EQ(u"Web app update with masking", names.to_name);

  // Verify the icons on the dialog.
  DialogIcons icons = GetIconsFromDialog(dialog_widget);
  EXPECT_THAT(icons.from_icon_bitmap,
              gfx::test::IsCloseToBitmap(
                  LoadExpectedBitmapFromDisk(GetFromIconFilePath()),
                  /*max_per_channel_deviation=*/3));
  EXPECT_THAT(icons.to_icon_bitmap,
              gfx::test::IsCloseToBitmap(
                  LoadExpectedBitmapFromDisk(GetToIconFilePath()),
                  /*max_per_channel_deviation=*/5));

  tester_.ExpectBucketCount("WrenchMenu.MenuAction",
                            MENU_ACTION_TRIGGER_APP_UPDATE_DIALOG, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateDialogBrowserTests, Accept) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  const webapps::AppId& app_id = InstallAppAndTriggerAppUpdateDialog();
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, dialog_widget);
  EXPECT_EQ("Web app for updating",
            provider().registrar_unsafe().GetAppShortName(app_id));

  BrowserWindowInterface* app_browser =
      AppBrowserController::FindForWebApp(*profile(), app_id);
  ASSERT_NE(app_browser, nullptr);
  BrowserView* app_browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  ASSERT_NE(app_browser_view, nullptr);
  // Verify that the dialog is showing in the browser, and that the menu button
  // is present and expanded.
  EXPECT_TRUE(app_browser_view->GetProperty(kIsPwaUpdateDialogShowingKey));
  WebAppMenuButton* const menu_button = views::AsViewClass<WebAppMenuButton>(
      app_browser_view->toolbar_button_provider()->GetAppMenuButton());
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());

  // Accept the dialog, and verify name was updated as part of updating from
  // `index.html` to `new_icon_page_masking.html`. Icons are also updated,
  // however that verification is skipped here for brevity. Tests for that are
  // part of the pending update application command unit-tests.
  {
    base::test::TestFuture<void> menu_update_future;
    base::CallbackListSubscription subscription =
        menu_button->AwaitLabelTextUpdated(
            menu_update_future.GetRepeatingCallback());
    views::test::AcceptDialog(dialog_widget);
    EXPECT_TRUE(menu_update_future.Wait());
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());
  EXPECT_EQ("Web app update with masking",
            provider().registrar_unsafe().GetAppShortName(app_id));
  EXPECT_THAT(tester_.GetAllSamples(kAppUpdateDialogResultHistogram),
              BucketsAre(base::Bucket(WebAppIdentityUpdateResult::kAccept, 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateDialogBrowserTests, CancelUninstall) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  const webapps::AppId& app_id = InstallAppAndTriggerAppUpdateDialog();
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, dialog_widget);

  // This will trigger uninstallation of the app. Wait for the uninstallation
  // dialog to show up.
  views::NamedWidgetShownWaiter uninstall_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  views::test::CancelDialog(dialog_widget);
  EXPECT_THAT(
      tester_.GetAllSamples(kAppUpdateDialogResultHistogram),
      BucketsAre(base::Bucket(WebAppIdentityUpdateResult::kUninstallApp, 1)));
  views::Widget* uninstall_dialog =
      uninstall_dialog_waiter.WaitIfNeededAndGet();

  // Trigger uninstallation of the app by accepting the dialog and verify.
  WebAppProvider* provider = WebAppProvider::GetForTest(profile());
  views::test::AcceptDialog(uninstall_dialog);
  provider->command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(provider->registrar_unsafe().IsInRegistrar(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateDialogBrowserTests, IgnoreRemovesMenuLabel) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  const webapps::AppId& app_id = InstallAppAndTriggerAppUpdateDialog();
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, dialog_widget);

  BrowserWindowInterface* app_browser =
      AppBrowserController::FindForWebApp(*profile(), app_id);
  ASSERT_NE(app_browser, nullptr);
  BrowserView* app_browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  ASSERT_NE(app_browser_view, nullptr);
  // Verify that the dialog is showing in the browser, and that the menu button
  // is present and expanded.
  EXPECT_TRUE(app_browser_view->GetProperty(kIsPwaUpdateDialogShowingKey));
  WebAppMenuButton* const menu_button = views::AsViewClass<WebAppMenuButton>(
      app_browser_view->toolbar_button_provider()->GetAppMenuButton());
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());

  // Trigger the ignore button and verify that the expanded label disappears.
  {
    base::test::TestFuture<void> menu_update_future;
    base::CallbackListSubscription subscription =
        menu_button->AwaitLabelTextUpdated(
            menu_update_future.GetRepeatingCallback());
    ClickIgnoreButtonOnDialog(dialog_widget);
    EXPECT_TRUE(menu_update_future.Wait());
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());
  EXPECT_THAT(tester_.GetAllSamples(kAppUpdateDialogResultHistogram),
              BucketsAre(base::Bucket(WebAppIdentityUpdateResult::kIgnore, 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateDialogBrowserTests,
                       NewPendingUpdatePostIgnoreExpandsButton) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  const webapps::AppId& app_id = InstallAppAndTriggerAppUpdateDialog();
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, dialog_widget);

  BrowserWindowInterface* app_browser =
      AppBrowserController::FindForWebApp(*profile(), app_id);
  ASSERT_NE(app_browser, nullptr);
  BrowserView* app_browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  ASSERT_NE(app_browser_view, nullptr);
  // Verify that the dialog is showing in the browser, and that the menu button
  // is present and expanded.
  EXPECT_TRUE(app_browser_view->GetProperty(kIsPwaUpdateDialogShowingKey));
  WebAppMenuButton* const menu_button = views::AsViewClass<WebAppMenuButton>(
      app_browser_view->toolbar_button_provider()->GetAppMenuButton());
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());

  // Trigger the ignore button and verify that the expanded label disappears.
  {
    base::test::TestFuture<void> menu_update_future;
    base::CallbackListSubscription subscription =
        menu_button->AwaitLabelTextUpdated(
            menu_update_future.GetRepeatingCallback());
    ClickIgnoreButtonOnDialog(dialog_widget);
    EXPECT_TRUE(menu_update_future.Wait());
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());

  // Trigger another update with security sensitive changes, verify that the
  // menu button is now in the expanded state with the label. The first update
  // was from index.html to new_icon_page_masking.html. Now, trigger an update
  // to new_icon_page.html.
  const GURL update_url2 =
      https_server()->GetURL("/web_apps/updating/new_icon_page.html");
  {
    base::test::TestFuture<void> update_future;
    UpdateAwaiter awaiter(provider().install_manager());
    auto subscription = menu_button->AwaitLabelTextUpdated(
        update_future.GetRepeatingCallback());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url2));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    EXPECT_TRUE(update_future.Wait());
  }
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());
}

}  // namespace
}  // namespace web_app

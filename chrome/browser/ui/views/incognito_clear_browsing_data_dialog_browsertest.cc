// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog_coordinator.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/isolated_mode/scoped_isolated_mode_override_for_testing.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "url/url_constants.h"

namespace {

class IncognitoClearBrowsingDataDialogTest : public InProcessBrowserTest {
 public:
  BrowserWindowInterface* CreateIncognitoBrowser(Profile* profile = nullptr) {
    BrowserWindowInterface* incognito_browser =
        InProcessBrowserTest::CreateIncognitoBrowser(profile);
    AvatarToolbarButtonTestAccessor(incognito_browser).WaitForAvatarButton();
    return incognito_browser;
  }

  void OpenDialog(IncognitoClearBrowsingDataDialog::Type type) {
    if (!incognito_browser_) {
      incognito_browser_ = CreateIncognitoBrowser(GetProfile());
    }

    auto* coordinator = GetCoordinator();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(incognito_browser_);
    coordinator->Show(type, browser_view->toolbar_button_provider()
                                ->GetAvatarToolbarButtonInterface());
    EXPECT_TRUE(coordinator->IsShowing());
  }

  BrowserWindowInterface* GetIncognitoBrowser() { return incognito_browser_; }

  IncognitoClearBrowsingDataDialog* GetDialogView() {
    return GetCoordinator()->GetIncognitoClearBrowsingDataDialogForTesting();
  }

  views::Widget* GetDialogWidget() {
    auto* dialog_view = GetDialogView();
    return dialog_view ? dialog_view->GetWidget() : nullptr;
  }

  IncognitoClearBrowsingDataDialogCoordinator* GetCoordinator() {
    return IncognitoClearBrowsingDataDialogCoordinator::From(
        incognito_browser_);
  }

  // Returns the first direct child of the shown dialog of type `T`, or nullptr
  // if there is none.
  template <typename T>
  T* GetDialogChildOfType() {
    auto* dialog_view = GetDialogView();
    CHECK(dialog_view);
    for (views::View* child : dialog_view->children()) {
      if (auto* typed_child = views::AsViewClass<T>(child)) {
        return typed_child;
      }
    }
    return nullptr;
  }

  // Returns whether the dialog shows the Incognito header art.
  bool DialogHasIncognitoHeaderArt() {
    return GetDialogChildOfType<ThemeTrackingNonAccessibleImageView>() !=
           nullptr;
  }

  // Returns whether the dialog shows the Isolated Mode header icon.
  bool DialogHasIsolatedHeaderIcon() {
    return GetDialogChildOfType<NonAccessibleImageView>() != nullptr;
  }

 private:
  raw_ptr<BrowserWindowInterface, AcrossTasksDanglingUntriaged>
      incognito_browser_ = nullptr;
};

// Used to test that the bubble widget is destroyed before the host browser.
class BubbleWidgetDestroyedObserver : public views::WidgetObserver {
 public:
  BubbleWidgetDestroyedObserver(BrowserWindowInterface* host_browser,
                                views::Widget* bubble_widget)
      : host_browser_(host_browser->GetWeakPtr()) {
    bubble_widget->AddObserver(this);
  }
  ~BubbleWidgetDestroyedObserver() override = default;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    ASSERT_TRUE(host_browser_);
  }

 private:
  base::WeakPtr<BrowserWindowInterface> host_browser_;
};

}  // namespace

class IncognitoClearBrowsingDataDialogBrowserTest
    : public SupportsTestDialog<IncognitoClearBrowsingDataDialogTest> {
 public:
  void ShowUi(const std::string& name) override {
    OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  }
};

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestDialogIsShown_DefaultBubble) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  auto* incognito_cbd_dialog_view = GetDialogView();

  ASSERT_TRUE(GetCoordinator()->IsShowing());
  ASSERT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            incognito_cbd_dialog_view->buttons());
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kCancel));
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestCloseWindowsButton) {
  base::HistogramTester histogram_tester;
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  auto destroyed_observer = BubbleWidgetDestroyedObserver(
      GetIncognitoBrowser(), GetDialogView()->GetWidget());

  ui_test_utils::BrowserDestroyedObserver observer(GetIncognitoBrowser());
  GetDialogView()->AcceptDialog();
  histogram_tester.ExpectBucketCount(
      "Incognito.ClearBrowsingDataDialog.ActionType",
      IncognitoClearBrowsingDataDialog::DialogActionType::kCloseIncognito, 1);
  observer.Wait();
  ASSERT_EQ(0u,
            GlobalBrowserCollection::GetInstance()->GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest, TestCancelButton) {
  base::HistogramTester histogram_tester;
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);

  views::test::WidgetDestroyedWaiter destroyed_waiter(GetDialogWidget());

  GetDialogView()->Cancel();
  histogram_tester.ExpectBucketCount(
      "Incognito.ClearBrowsingDataDialog.ActionType",
      IncognitoClearBrowsingDataDialog::DialogActionType::kCancel, 1);
  destroyed_waiter.Wait();

  ASSERT_FALSE(GetCoordinator()->IsShowing());
  ASSERT_FALSE(GetDialogView());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestBrowserCloseEventClosesDialogFirst) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  auto destroyed_observer =
      BubbleWidgetDestroyedObserver(GetIncognitoBrowser(), GetDialogWidget());

  CloseBrowserSynchronously(GetIncognitoBrowser());

  ASSERT_EQ(0u,
            GlobalBrowserCollection::GetInstance()->GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       ClearBrowsingDataNavigationInIncognito) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::SendToOmniboxAndSubmit(incognito_browser,
                                        "chrome://settings/clearBrowserData");
  std::u16string current_tab_title;
  ui_test_utils::GetCurrentTabTitle(incognito_browser, &current_tab_title);
  EXPECT_EQ(u"about:blank", current_tab_title);
  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(incognito_browser);
  ASSERT_TRUE(coordinator->IsShowing());
}

class IsolatedClearBrowsingDataDialogTest
    : public IncognitoClearBrowsingDataDialogTest {
 private:
  enterprise_isolated_mode::ScopedIsolatedModeOverrideForTesting
      scoped_isolated_mode_;
};

IN_PROC_BROWSER_TEST_F(IsolatedClearBrowsingDataDialogTest,
                       ClearBrowsingDataNavigationInIsolatedMode) {
  BrowserWindowInterface* isolated_browser = CreateIncognitoBrowser();
  Profile* isolated_profile = isolated_browser->GetProfile();
  ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());
  ui_test_utils::SendToOmniboxAndSubmit(isolated_browser,
                                        "chrome://settings/clearBrowserData");
  std::u16string current_tab_title;
  ui_test_utils::GetCurrentTabTitle(isolated_browser, &current_tab_title);
  EXPECT_EQ(u"about:blank", current_tab_title);
  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(isolated_browser);
  ASSERT_TRUE(coordinator->IsShowing());
}

IN_PROC_BROWSER_TEST_F(IsolatedClearBrowsingDataDialogTest,
                       HistoryNavigationInIsolatedMode) {
  BrowserWindowInterface* isolated_browser = CreateIncognitoBrowser();
  Profile* isolated_profile = isolated_browser->GetProfile();
  ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());
  ui_test_utils::SendToOmniboxAndSubmit(isolated_browser, "chrome://history");
  std::u16string current_tab_title;
  ui_test_utils::GetCurrentTabTitle(isolated_browser, &current_tab_title);
  EXPECT_EQ(u"about:blank", current_tab_title);
  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(isolated_browser);
  ASSERT_TRUE(coordinator->IsShowing());
}

IN_PROC_BROWSER_TEST_F(IsolatedClearBrowsingDataDialogTest,
                       IsolatedHeaderIconShownInIsolatedMode) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  ASSERT_TRUE(
      GetIncognitoBrowser()->GetProfile()->IsEnterpriseIsolatedModeProfile());
  auto* icon = GetDialogChildOfType<NonAccessibleImageView>();
  ASSERT_TRUE(icon);
  EXPECT_FALSE(DialogHasIncognitoHeaderArt());

  // The icon shows the Isolated Mode header art.
  const gfx::ImageSkia* isolated_header =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_ISOLATED_DATA_NOT_SAVED_HEADER);
  ASSERT_TRUE(isolated_header);
  EXPECT_TRUE(
      icon->GetImageModel().GetImage().AsImageSkia().BackedBySameObjectAs(
          *isolated_header));

  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);
  EXPECT_TRUE(DialogHasIsolatedHeaderIcon());
  EXPECT_FALSE(DialogHasIncognitoHeaderArt());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       IncognitoHeaderArtShownInIncognito) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  ASSERT_TRUE(GetIncognitoBrowser()->GetProfile()->IsIncognitoProfile());
  EXPECT_TRUE(DialogHasIncognitoHeaderArt());
  EXPECT_FALSE(DialogHasIsolatedHeaderIcon());

  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);
  EXPECT_TRUE(DialogHasIncognitoHeaderArt());
  EXPECT_FALSE(DialogHasIsolatedHeaderIcon());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestDialogIsShown_HistoryDisclaimerBubble) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);
  auto* incognito_cbd_dialog_view = GetDialogView();

  ASSERT_TRUE(GetCoordinator()->IsShowing());
  ASSERT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            incognito_cbd_dialog_view->buttons());
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kCancel));
}

// Regression test for crbug.com/589703940: the labels must fit in the space
// the bubble gives them, without eliding or clipping.
IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       HistoryDisclaimerBubbleLabelsFit) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);
  views::test::RunScheduledLayout(GetDialogWidget());

  int label_count = 0;
  for (views::View* child : GetDialogView()->children()) {
    auto* label = views::AsViewClass<views::Label>(child);
    if (!label) {
      continue;
    }
    ++label_count;
    SCOPED_TRACE(label->GetText());
    EXPECT_FALSE(label->IsDisplayTextTruncated());
    const gfx::Size needed =
        label->GetPreferredSize(views::SizeBounds(label->width(), {}));
    EXPECT_LE(needed.width(), label->width());
    EXPECT_LE(needed.height(), label->height());
  }
  EXPECT_EQ(2, label_count);
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestCloseIncognitoButton) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);

  ui_test_utils::BrowserDestroyedObserver observer(GetIncognitoBrowser());
  GetDialogView()->CancelDialog();
  observer.Wait();
  ASSERT_EQ(0u,
            GlobalBrowserCollection::GetInstance()->GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest, TestGotItButton) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);

  views::test::WidgetDestroyedWaiter destroyed_waiter(GetDialogWidget());

  GetDialogView()->AcceptDialog();
  destroyed_waiter.Wait();

  ASSERT_FALSE(GetCoordinator()->IsShowing());
  ASSERT_FALSE(GetDialogView());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest, ShowTwice) {
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);
  auto* const first_incognito_cbd_dialog_view = GetDialogView();
  EXPECT_TRUE(first_incognito_cbd_dialog_view);

  // `view_tracker_for_first_view` is used to check if
  // `first_incognito_cbd_dialog_view` has been released.
  views::ViewTracker view_tracker_for_first_view(
      first_incognito_cbd_dialog_view);

  // Show again with the same type.
  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble);
  const auto* const second_incognito_cbd_dialog_view = GetDialogView();
  EXPECT_TRUE(second_incognito_cbd_dialog_view);

  // Two views should not be the same.
  EXPECT_NE(first_incognito_cbd_dialog_view, second_incognito_cbd_dialog_view);

  views::test::WidgetDestroyedWaiter destroyed_waiter(GetDialogWidget());

  GetDialogView()->AcceptDialog();
  destroyed_waiter.Wait();

  // `first_incognito_cbd_dialog_view` should already have been released when
  // the second dialog is created.
  EXPECT_FALSE(view_tracker_for_first_view.view());

  ASSERT_FALSE(GetCoordinator()->IsShowing());
  ASSERT_FALSE(GetDialogView());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       ShowWithNullAvatarButton) {
  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(GetProfile());
  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(incognito_browser);
  coordinator->Show(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble,
                    nullptr);
  EXPECT_FALSE(coordinator->IsShowing());

  OpenDialog(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble);
  ASSERT_TRUE(GetCoordinator()->IsShowing());
  views::test::WidgetDestroyedWaiter destroyed_waiter(GetDialogWidget());

  GetCoordinator()->Show(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble,
                         nullptr);
  EXPECT_FALSE(GetCoordinator()->IsShowing());
  EXPECT_FALSE(GetDialogView());
  destroyed_waiter.Wait();
}

class IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest
    : public IncognitoClearBrowsingDataDialogTest {
 public:
  IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIToolbar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2},
        {});
  }

  bool WaitForAvatarElementVisible(BrowserWindowInterface* browser,
                                   bool expected_visible) {
    return base::test::RunUntil([&]() {
      return (BrowserElements::From(browser)->GetElement(
                  kToolbarAvatarButtonElementId) != nullptr) ==
             expected_visible;
    });
  }

  void OverflowWebUIToolbar(BrowserWindowInterface* browser) {
    ASSERT_TRUE(WaitForAvatarElementVisible(browser, true));
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    WebUIToolbarWebView* webui_toolbar =
        browser_view->toolbar()->GetWebUIToolbarViewForTesting();
    ASSERT_TRUE(webui_toolbar);
    const int spacer_width = browser_view->GetBounds().width();
    ASSERT_EQ(true, content::EvalJs(
                        webui_toolbar->GetWebViewForTesting()->GetWebContents(),
                        content::JsReplace(
                            R"((() => {
            const app = document.querySelector('toolbar-app');
            let spacer = app.shadowRoot.querySelector('#test-spacer');
            if (!spacer) {
              spacer = document.createElement('div');
              spacer.id = 'test-spacer';
              spacer.style.flexShrink = '0';
              app.shadowRoot.appendChild(spacer);
            }
            spacer.style.width = $1 + 'px';
            app.layoutResponsiveControls();
            return true;
          })();)",
                            spacer_width)));
    ASSERT_TRUE(WaitForAvatarElementVisible(browser, false));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest,
                       ShowBeforeAvatarButtonReady) {
  BrowserWindowInterface* incognito_browser = CreateBrowserWindowForTesting(
      BrowserWindowCreateParams(GetProfile()->GetPrimaryOTRProfile(
                                    /*create_if_needed=*/true),
                                /*from_user_gesture=*/true));
  chrome::AddSelectedTabWithURL(incognito_browser, GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser);
  browser_view->Show();

  // Before the WebUI toolbar finishes initial load (prior to
  // WaitForAvatarButton()), the avatar TrackedElement does not exist yet.
  ASSERT_FALSE(BrowserElements::From(incognito_browser)
                   ->GetElement(kToolbarAvatarButtonElementId));

  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(incognito_browser);
  coordinator->Show(
      IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble,
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface());
  EXPECT_FALSE(coordinator->IsShowing());

  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));
  ui::TrackedElement* avatar_element =
      BrowserElements::From(incognito_browser)
          ->GetElement(kToolbarAvatarButtonElementId);
  ASSERT_TRUE(avatar_element);

  auto* dialog_view =
      coordinator->GetIncognitoClearBrowsingDataDialogForTesting();
  ASSERT_TRUE(dialog_view);
  EXPECT_EQ(dialog_view->GetAnchorRect(), avatar_element->GetScreenBounds());

  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_view->GetWidget());
  dialog_view->AcceptDialog();
  destroyed_waiter.Wait();

  EXPECT_FALSE(coordinator->IsShowing());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest,
                       ShowWhenAvatarOverflowedPopsOutAndRestoresOnClose) {
  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(GetProfile());
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(incognito_browser));

  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(incognito_browser);
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser);
  coordinator->Show(
      IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble,
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface());
  EXPECT_FALSE(coordinator->IsShowing());

  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));
  ui::TrackedElement* avatar_element =
      BrowserElements::From(incognito_browser)
          ->GetElement(kToolbarAvatarButtonElementId);
  ASSERT_TRUE(avatar_element);

  auto* dialog_view =
      coordinator->GetIncognitoClearBrowsingDataDialogForTesting();
  ASSERT_TRUE(dialog_view);
  EXPECT_EQ(dialog_view->GetAnchorRect(), avatar_element->GetScreenBounds());

  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_view->GetWidget());
  dialog_view->AcceptDialog();
  destroyed_waiter.Wait();

  EXPECT_FALSE(coordinator->IsShowing());
  EXPECT_TRUE(WaitForAvatarElementVisible(incognito_browser, false));
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest,
                       ShowTwiceWhilePendingCancelsFirstRequest) {
  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(GetProfile());
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(incognito_browser));

  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(incognito_browser);
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser);
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();

  int shown_dialog_count = 0;
  views::AnyWidgetObserver widget_observer(views::test::AnyWidgetTestPasskey{});
  widget_observer.set_shown_callback(base::BindRepeating(
      [](int* count, views::Widget* widget) {
        if (widget->GetName() == "IncognitoClearBrowsingDataDialog") {
          ++(*count);
        }
      },
      &shown_dialog_count));

  coordinator->Show(IncognitoClearBrowsingDataDialog::Type::kDefaultBubble,
                    avatar_button);
  coordinator->Show(
      IncognitoClearBrowsingDataDialog::Type::kHistoryDisclaimerBubble,
      avatar_button);
  EXPECT_FALSE(coordinator->IsShowing());

  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));
  EXPECT_EQ(1, shown_dialog_count);
  auto* dialog_view =
      coordinator->GetIncognitoClearBrowsingDataDialogForTesting();
  ASSERT_TRUE(dialog_view);
  EXPECT_EQ(dialog_view->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
            l10n_util::GetStringUTF16(
                IDS_INCOGNITO_HISTORY_BUBBLE_CANCEL_BUTTON_TEXT));

  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_view->GetWidget());
  dialog_view->AcceptDialog();
  destroyed_waiter.Wait();

  EXPECT_FALSE(coordinator->IsShowing());
  EXPECT_TRUE(WaitForAvatarElementVisible(incognito_browser, false));
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest,
                       CloseBrowserWhilePendingAndWhileShowing) {
  // Closing the Incognito browser while RunWhenVisible() is still pending
  // cancels the callback cleanly without UAF.
  BrowserWindowInterface* pending_browser =
      CreateIncognitoBrowser(GetProfile());
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(pending_browser));

  auto* pending_coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(pending_browser);
  BrowserView* pending_browser_view =
      BrowserView::GetBrowserViewForBrowser(pending_browser);
  pending_coordinator->Show(
      IncognitoClearBrowsingDataDialog::Type::kDefaultBubble,
      pending_browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface());
  EXPECT_FALSE(pending_coordinator->IsShowing());

  CloseBrowserSynchronously(pending_browser);
  ASSERT_EQ(0u,
            GlobalBrowserCollection::GetInstance()->GetIncognitoBrowserCount());

  // Clicking "Close windows" while the dialog holds a live keep_visible runner
  // tears down the dialog and browser cleanly.
  BrowserWindowInterface* showing_browser =
      CreateIncognitoBrowser(GetProfile());
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(showing_browser));

  auto* showing_coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(showing_browser);
  BrowserView* showing_browser_view =
      BrowserView::GetBrowserViewForBrowser(showing_browser);
  showing_coordinator->Show(
      IncognitoClearBrowsingDataDialog::Type::kDefaultBubble,
      showing_browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return showing_coordinator->IsShowing(); }));

  ui_test_utils::BrowserDestroyedObserver observer(showing_browser);
  showing_coordinator->GetIncognitoClearBrowsingDataDialogForTesting()
      ->AcceptDialog();
  observer.Wait();
  ASSERT_EQ(0u,
            GlobalBrowserCollection::GetInstance()->GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogWebUIAvatarOverflowTest,
                       ClearBrowsingDataNavigationWhenAvatarOverflowed) {
  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(GetProfile());
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(incognito_browser));

  ui_test_utils::SendToOmniboxAndSubmit(incognito_browser,
                                        "chrome://settings/clearBrowserData");
  auto* coordinator =
      IncognitoClearBrowsingDataDialogCoordinator::From(incognito_browser);
  ASSERT_TRUE(base::test::RunUntil([&]() { return coordinator->IsShowing(); }));
  EXPECT_TRUE(WaitForAvatarElementVisible(incognito_browser, true));

  auto* dialog_view =
      coordinator->GetIncognitoClearBrowsingDataDialogForTesting();
  ASSERT_TRUE(dialog_view);
  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog_view->GetWidget());
  dialog_view->Cancel();
  destroyed_waiter.Wait();

  EXPECT_FALSE(coordinator->IsShowing());
  EXPECT_TRUE(WaitForAvatarElementVisible(incognito_browser, false));
}

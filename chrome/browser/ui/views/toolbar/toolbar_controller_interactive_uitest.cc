// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <sstream>
#include <variant>

#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/test_extension_dir.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

namespace {
constexpr int kBrowserContentAllowedMinimumWidth =
    BrowserViewLayout::kMainBrowserContentsMinimumWidth;
}  // namespace

// The bool indicates whether to use WebUI controls for the buttons on the left
// of the toolbar.
class ToolbarControllerUiTest : public InteractiveFeaturePromoTest,
                                public testing::WithParamInterface<bool> {
 public:
  ToolbarControllerUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHMemorySaverModeFeature})) {
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
    std::vector<base::test::FeatureRef> web_ui_features = {
        features::kInitialWebUI, features::kWebUIReloadButton,
        features::kWebUIBackForwardButton, features::kWebUISplitTabsButton,
        features::kWebUIHomeButton};
    if (WebUIButtonsEnabled()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/web_ui_features,
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/web_ui_features);
    }
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, false);
    if (tabs::GetTabSearchPosition(browser()) ==
        tabs::TabSearchPosition::kToolbarButton) {
      actions_model->UpdatePinnedState(kActionTabSearch, false);
    }
    CHECK(!features::IsWebUIPinnedToolbarActionsEnabled())
        << "Test needs modification to support WebUIPinnedToolbarActions";
    views::test::WaitForAnimatingLayoutManager(
        static_cast<PinnedToolbarActionsContainer*>(
            browser_view_->toolbar_button_provider()
                ->GetPinnedToolbarActions()));
    toolbar_controller_ = const_cast<ToolbarController*>(
        browser_view_->toolbar()->toolbar_controller());
    toolbar_container_view_ = const_cast<views::View*>(
        toolbar_controller_->toolbar_container_view_.get());
    overflow_button_ = toolbar_controller_->overflow_button_;
    dummy_button_size_ = overflow_button_->GetPreferredSize();
    element_flex_order_start_ = toolbar_controller_->element_flex_order_start_;
    MaybeAddDummyButtonsToToolbarView();

    // On some platforms, the toolbar may not be able to overflow, because the
    // minimum size doesn't allow any elements to drop out. When this happens,
    // this will be null, and overflow tests will be skipped.
    const int overflow_threshold =
        GetOverflowThresholdWidthInToolbarContainer();

    overflow_threshold_width_ =
        overflow_threshold <= toolbar_container_view_->GetMinimumSize().width()
            ? std::nullopt
            : std::make_optional(overflow_threshold);

    default_browser_width_ = browser()->GetWindow()->GetBounds().width();
    ASSERT_GT(default_browser_width_, overflow_threshold_width_);
  }

  void TearDownOnMainThread() override {
    toolbar_container_view_ = nullptr;
    overflow_button_ = nullptr;
    toolbar_controller_ = nullptr;
    browser_view_ = nullptr;
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

  bool WebUIButtonsEnabled() const { return GetParam(); }

  // Returns the minimum width the toolbar view can be without any ToolbarButton
  // dropped out in ToolbarContainer. This function calculates the
  // browser width where elements with flex order > kOrderOffset defined in
  // ToolbarView should have minimum size. Since elements with flex order <=
  // kOrderOffset happens to have minimum width == preferred width it has no
  // effect on diff_sum.
  int GetOverflowThresholdWidthInToolbarContainer() {
    int diff_sum = 0;
    for (views::View* element : toolbar_container_view_->children()) {
      diff_sum += element->GetPreferredSize().width() -
                  element->GetMinimumSize().width();
    }
    return toolbar_container_view_->GetPreferredSize().width() - diff_sum;
  }

  // Returns the minimum width the toolbar view can be without any elements
  // dropped out in PinnedSidePanelContainer. This function calculates the
  // browser width where elements with flex order > kToolbarActionsFlexOrder
  // defined in ToolbarView should have minimum size
  int GetOverflowThresholdWidthInPinnedSidePanelContainer() {
    auto* extensions_container =
        browser_view_->toolbar()->extensions_container();
    int diff = extensions_container->GetPreferredSize().width() -
               extensions_container->GetMinimumSize().width();
    return toolbar_container_view_->GetPreferredSize().width() - diff;
  }

  // Because actual_browser_minimum_width == Max(toolbar_width,
  // kBrowserContentAllowedMinimumWidth) So if `overflow_threshold_width_` <
  // kBrowserContentAllowedMinimumWidth, then actual_browser_minimum_width ==
  // kBrowserContentAllowedMinimumWidth In this case we will never see any
  // overflow so stuff toolbar with some fixed dummy buttons till it's
  // guaranteed we can observe overflow with browser resized to its minimum
  // width.
  void MaybeAddDummyButtonsToToolbarView() {
    while (GetOverflowThresholdWidthInToolbarContainer() <=
           kBrowserContentAllowedMinimumWidth) {
      toolbar_container_view_->AddChildView(CreateADummyButton());
    }
  }

  std::unique_ptr<ToolbarButton> CreateADummyButton() {
    auto button = std::make_unique<ToolbarButton>();
    button->SetPreferredSize(dummy_button_size_);
    button->SetMinSize(dummy_button_size_);
    button->GetViewAccessibility().SetName(u"dummybutton");
    button->SetVisible(true);
    return button;
  }

  auto CheckIsManagedByController(ui::ElementIdentifier id) {
    return Check(
        [this, id]() {
          const std::vector<ToolbarController::ResponsiveElementInfo>&
              responsive_elements = get_responsive_elements();
          for (const auto& el : responsive_elements) {
            if (const auto* info =
                    std::get_if<ToolbarController::ElementIdInfo>(
                        &el.overflow_id)) {
              if (info->overflow_identifier == id) {
                return true;
              }
            }
          }
          return false;
        },
        "CheckIsManagedByController()");
  }

  auto CheckIsManagedByController(actions::ActionId id) {
    return Check(
        [this, id]() {
          const std::vector<ToolbarController::ResponsiveElementInfo>&
              responsive_elements = get_responsive_elements();
          for (const auto& el : responsive_elements) {
            if (const auto* action =
                    std::get_if<actions::ActionId>(&el.overflow_id)) {
              if (*action == id) {
                return true;
              }
            }
          }
          return false;
        },
        "CheckIsManagedByController()");
  }

  auto CheckActionItemOverflowed(actions::ActionId id, bool overflowed) {
    return CheckResult([this, id]() { return delegate()->IsOverflowed(id); },
                       overflowed,
                       base::StringPrintf("CheckActionItemOverflowed(%s)",
                                          base::ToString(overflowed)));
  }

  auto CheckIfOverflowed(ui::ElementIdentifier id, bool is_overflowed) {
    return CheckResult(
        [this, id]() {
          return toolbar_controller_->IsElementOverflowedForTesting(id);
        },
        is_overflowed,
        base::StringPrintf("CheckIfOverflowed(%s)",
                           base::ToString(is_overflowed)));
  }

  auto SetBooleanPref(const std::string& path, bool value) {
    return Do([this, path, value]() {
      browser()->profile()->GetPrefs()->SetBoolean(path, value);
    });
  }

  // Waits until an overflowable element is visible.
  MultiStep WaitForElementVisibility(ui::ElementIdentifier id,
                                     bool visibility) {
    if (WebUIButtonsEnabled()) {
      std::optional<WebContentsInteractionTestUtil::DeepQuery> query;
      if (id == kToolbarForwardButtonElementId) {
        query = WebContentsInteractionTestUtil::DeepQuery{
            "toolbar-app", "back-forward-button#forward"};
      } else if (id == kToolbarHomeButtonElementId) {
        query = WebContentsInteractionTestUtil::DeepQuery{"toolbar-app",
                                                          "home-button"};
      }
      if (query) {
        return WaitForJsResultAt(kWebUIToolbarId, *query,
                                 R"(el => (!el.hidden))", visibility);
      }
    }

    if (visibility) {
      return Steps(WaitForShow(id));
    } else {
      return Steps(WaitForHide(id));
    }
  }

  // Forces `id` to overflow by filling toolbar with dummy buttons.
  auto AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(
      ui::ElementIdentifier id) {
    auto result = Steps(
        CheckIsManagedByController(id),
        Do([this, id]() {
          while (!toolbar_controller_->IsElementOverflowedForTesting(id)) {
            toolbar_container_view_->AddChildView(CreateADummyButton());
            views::test::RunScheduledLayout(browser_view_);
          }
        }).SetDescription("ForceOverflow"),
        WaitForShow(kToolbarOverflowButtonElementId), WaitForHide(id));
    AddDescriptionPrefix(
        result,
        "AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing()");
    return result;
  }

  // Forces `id` to overflow by filling toolbar with dummy buttons. Shrinks
  // Window to minimum size first.
  auto AddDummyButtonsToToolbarTillElementOverflows(ui::ElementIdentifier id) {
    auto result =
        Steps(Do([this]() {
                SetBrowserWidth(kBrowserContentAllowedMinimumWidth);
              }).SetDescription("SetBrowserWidth()"),
              AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(id));
    AddDescriptionPrefix(result,
                         "AddDummyButtonsToToolbarTillElementOverflows()");
    return result;
  }

  auto AddDummyButtonsToToolbarTillElementOverflows(actions::ActionId id) {
    auto result = Steps(CheckIsManagedByController(id),
                        Do([this]() {
                          SetBrowserWidth(kBrowserContentAllowedMinimumWidth);
                        }).SetDescription("SetBrowserWidth()"),
                        Do([this, id]() {
                          while (!delegate()->IsOverflowed(id)) {
                            toolbar_container_view_->AddChildView(
                                CreateADummyButton());
                            views::test::RunScheduledLayout(browser_view_);
                          }
                        }).SetDescription("ForceOverflow"),
                        WaitForShow(kToolbarOverflowButtonElementId),
                        CheckActionItemOverflowed(id, true));
    AddDescriptionPrefix(result,
                         "AddDummyButtonsToToolbarTillElementOverflows()");
    return result;
  }

  // This checks menu model, not the actual menu that pops up.
  // TODO(pengchaocai): Explore a way to check the actual menu appearing.
  auto CheckMenuMatchesOverflowedElements() {
    return Steps(Check([this]() {
      const ui::SimpleMenuModel* menu = GetOverflowMenu();
      EXPECT_NE(menu, nullptr);
      EXPECT_GT(menu->GetItemCount(), size_t(0));
      const auto& responsive_elements = get_responsive_elements();
      for (size_t i = 0; i < responsive_elements.size(); ++i) {
        if (toolbar_controller_->IsOverflowed(responsive_elements[i])) {
          if (toolbar_controller_->GetMenuText(responsive_elements[i]) !=
              menu->GetLabelAt(menu->GetIndexOfCommandId(i).value())) {
            return false;
          }
        }
      }
      return true;
    }));
  }

  auto ActivateMenuItemWithElementId(
      std::variant<ui::ElementIdentifier, actions::ActionId> id) {
    return Do([=, this]() {
      const std::vector<ToolbarController::ResponsiveElementInfo>&
          responsive_elements = get_responsive_elements();
      int command_id = -1;
      for (size_t i = 0; i < responsive_elements.size(); ++i) {
        const auto& overflow_id = responsive_elements[i].overflow_id;
        std::visit(
            absl::Overload(
                [&](ToolbarController::ElementIdInfo overflow_id) {
                  if (std::holds_alternative<ui::ElementIdentifier>(id) &&
                      overflow_id.overflow_identifier ==
                          std::get<ui::ElementIdentifier>(id)) {
                    command_id = i;
                    return;
                  }
                },
                [&](actions::ActionId overflow_id) {
                  if (std::holds_alternative<actions::ActionId>(id) &&
                      overflow_id == std::get<actions::ActionId>(id)) {
                    command_id = i;
                    return;
                  }
                }),
            overflow_id);
        if (command_id != -1) {
          break;
        }
      }
      auto* menu = const_cast<ui::SimpleMenuModel*>(GetOverflowMenu());
      auto index = menu->GetIndexOfCommandId(command_id);
      EXPECT_TRUE(index.has_value());
      menu->ActivatedAt(index.value());
    });
  }

  auto ForceForwardButtonOverflow() {
    return Steps(AddDummyButtonsToToolbarTillElementOverflows(
        kToolbarForwardButtonElementId));
  }

  auto PinBookmarkToToolbar() {
    return Steps(Do([=, this]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_BOOKMARK_SIDE_PANEL);
                 }),
                 WaitForShow(kSidePanelElementId),
                 PressButton(kSidePanelPinButtonElementId),
                 PressButton(kSidePanelCloseButtonElementId),
                 WaitForHide(kSidePanelElementId));
  }

  auto PinReadingModeToToolbar() {
    return Steps(Do([=, this]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_READING_MODE_SIDE_PANEL);
                 }),
                 WaitForShow(kSidePanelElementId),
                 PressButton(kSidePanelPinButtonElementId),
                 PressButton(kSidePanelCloseButtonElementId),
                 WaitForHide(kSidePanelElementId));
  }

  auto RestoreBrowserWidth() {
    return Steps(Do([this]() { SetBrowserWidth(default_browser_width_); }),
                 WaitForHide(kToolbarOverflowButtonElementId));
  }

  auto LoadAndPinExtensionButton() {
    return Do([this]() {
      extensions::TestExtensionDir extension_directory;
      constexpr char kManifest[] = R"({
        "name": "Test Extension",
        "version": "1",
        "manifest_version": 3,
        "host_permissions": [
          "<all_urls>"
        ]
      })";
      extension_directory.WriteManifest(kManifest);
      extensions::ChromeTestExtensionLoader loader(browser()->profile());
      scoped_refptr<const extensions::Extension> extension =
          loader.LoadExtension(extension_directory.UnpackedPath());

      // Pin extension.
      auto* toolbar_model = ToolbarActionsModel::Get(browser()->profile());
      ASSERT_TRUE(toolbar_model);
      toolbar_model->SetActionVisibility(extension->id(), true);
      views::test::RunScheduledLayout(browser_view_);
    });
  }

  auto ResizeRelativeToOverflow(int diff) {
    return Do([this, diff]() {
      SetBrowserWidth(*overflow_threshold_width() + diff);
    });
  }

  void SetBrowserWidth(int width) {
    int widget_width = browser_view_->GetWidget()->GetSize().width();
    int browser_width = browser_view_->size().width();
    browser_view_->GetWidget()->SetSize(
        {width + widget_width - browser_width,
         browser_view_->GetWidget()->GetSize().height()});
    views::test::RunScheduledLayout(browser_view_);
  }

  const views::View* FindToolbarElementWithId(ui::ElementIdentifier id) const {
    return toolbar_controller_->FindToolbarElementWithId(
        toolbar_container_view_, id);
  }

  MultiStep InstrumentToolbarWebUiIfNeeded() {
    if (WebUIButtonsEnabled()) {
      return InstrumentNonTabWebView(kWebUIToolbarId,
                                     browser_view_->toolbar_button_provider()
                                         ->GetWebUIToolbarViewForTesting()
                                         ->GetWebViewForTesting());
    }
    return Steps();
  }

  gfx::Size dummy_button_size() { return dummy_button_size_; }
  ToolbarController::PinnedActionsDelegate* delegate() {
    return toolbar_controller_->pinned_actions_delegate_;
  }
  const views::View* overflow_button() const { return overflow_button_; }
  int element_flex_order_start() const { return element_flex_order_start_; }
  const std::vector<ToolbarController::ResponsiveElementInfo>&
  get_responsive_elements() const {
    return toolbar_controller_->responsive_elements_;
  }
  std::optional<int> overflow_threshold_width() const {
    return overflow_threshold_width_;
  }
  std::vector<const ToolbarController::ResponsiveElementInfo*>
  GetOverflowedElements() {
    return toolbar_controller_->GetOverflowedElements();
  }
  const ui::SimpleMenuModel* GetOverflowMenu() {
    return toolbar_controller_->menu_model_for_testing();
  }
  BrowserView* browser_view() { return browser_view_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<BrowserView> browser_view_;
  raw_ptr<ToolbarController> toolbar_controller_;
  raw_ptr<views::View> toolbar_container_view_;
  raw_ptr<views::View> overflow_button_;
  int element_flex_order_start_;
  gfx::Size dummy_button_size_;

  // The minimum width the toolbar view can be without any elements dropped out.
  std::optional<int> overflow_threshold_width_;

  int default_browser_width_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ToolbarControllerUiTest,
    ::testing::Bool());

// TODO(crbug.com/41495158): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_StartBrowserWithThresholdWidth \
  DISABLED_StartBrowserWithThresholdWidth
#else
#define MAYBE_StartBrowserWithThresholdWidth StartBrowserWithThresholdWidth
#endif
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       MAYBE_StartBrowserWithThresholdWidth) {
  const auto threshold = overflow_threshold_width();
  if (!threshold) {
    GTEST_SKIP();
  }

  // Start browser with threshold width. Should not see overflow.
  SetBrowserWidth(*threshold);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should not see overflow.
  SetBrowserWidth(*threshold + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(*threshold);
  EXPECT_FALSE(overflow_button()->GetVisible());

  base::UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonShown"));

  // Resize browser a bit narrower. Should see overflow.
  SetBrowserWidth(*threshold - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonShown"));

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonHidden"));

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(*threshold);
  EXPECT_FALSE(overflow_button()->GetVisible());

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonHidden"));
}
// TODO(crbug.com/41495158): Flaky on Windows.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StartBrowserWithWidthSmallerThanThreshold \
  DISABLED_StartBrowserWithWidthSmallerThanThreshold
#else
#define MAYBE_StartBrowserWithWidthSmallerThanThreshold \
  StartBrowserWithWidthSmallerThanThreshold
#endif
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       MAYBE_StartBrowserWithWidthSmallerThanThreshold) {
  const auto threshold = overflow_threshold_width();
  if (!threshold) {
    GTEST_SKIP();
  }

  // Start browser with a smaller width than threshold. Should see overflow.
  SetBrowserWidth(*threshold - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser wider to threshold width. Should not see overflow.
  SetBrowserWidth(*threshold);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should see overflow.
  SetBrowserWidth(*threshold - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Keep resizing browser narrower. Should see overflow.
  SetBrowserWidth(*threshold - 2);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should still see overflow.
  SetBrowserWidth(*threshold - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       StartBrowserWithWidthLargerThanThreshold) {
  const auto threshold = overflow_threshold_width();
  if (!threshold) {
    GTEST_SKIP();
  }

  // Start browser with a larger width than threshold. Should not see overflow.
  SetBrowserWidth(*threshold + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser wider. Should not see overflow.
  SetBrowserWidth(*threshold + 2);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should not see overflow.
  SetBrowserWidth(*threshold + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(*threshold);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should not see overflow.
  SetBrowserWidth(*threshold + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest, MenuMatchesOverflowedElements) {
  const auto threshold = overflow_threshold_width();
  if (!threshold) {
    GTEST_SKIP();
  }

  RunTestSequence(Do([this, threshold]() { SetBrowserWidth(*threshold - 1); }),
                  WaitForShow(kToolbarOverflowButtonElementId),
                  PressButton(kToolbarOverflowButtonElementId),
                  CheckMenuMatchesOverflowedElements());
}

// Tests that the home and overflow buttons are always hidden together, when
// they're the two lowest priority hideable buttons.
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest, HomeForwardOverflowTogether) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

  RunTestSequence(
      InstrumentToolbarWebUiIfNeeded(),
      WaitForElementVisibility(kToolbarHomeButtonElementId, true),
      CheckIfOverflowed(kToolbarHomeButtonElementId, false),
      // Home button overflows before the forward button, so add buttons until
      // the home button overflows.
      //
      // AddDummyButtonsToToolbarTillElementOverflows() will resize the window
      // first. Since dummy buttons were already added, based on when the
      // forward button would be hidden if the Window was set to min size, and
      // we've added the home button into the the calculations now, it's better
      // not to use the resizing version of the function, which could hide both
      // buttons at once just due to all the dummy buttons on the toolbar not
      // leaving space for either extra button on the toolbar, rather than due
      // to not having enough space for the home button resulting in having to
      // show the overflow button, which means there no space left for the
      // forward button, either.
      AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(
          kToolbarHomeButtonElementId),
      // Check that the forward button has also overflowed, and that the home
      // and forward buttons are both not visible, which is consistent with
      // having overflowed.
      CheckIfOverflowed(kToolbarForwardButtonElementId, true),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, false),

      // Remove the last dummy button. The forward and home buttons should be
      // visible and no longer overflowed, and the overflow button should be
      // hidden, once a new layout occurs.
      Do([&]() {
        std::unique_ptr<views::View> removed_view =
            toolbar_container_view_->RemoveChildViewT(
                toolbar_container_view_->children().back());
      }),
      // That should trigger a new layout of the toolbar. Wait for visual update
      // of the buttons.
      WaitForElementVisibility(kToolbarHomeButtonElementId, true),
      WaitForElementVisibility(kToolbarForwardButtonElementId, true),
      WaitForHide(kToolbarOverflowButtonElementId),
      CheckIfOverflowed(kToolbarHomeButtonElementId, false),
      CheckIfOverflowed(kToolbarForwardButtonElementId, false));
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest, HomeForwardOverflowSeparately) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

  RunTestSequence(
      InstrumentToolbarWebUiIfNeeded(), PinBookmarkToToolbar(),
      WaitForElementVisibility(kToolbarHomeButtonElementId, true),
      WaitForElementVisibility(kToolbarForwardButtonElementId, true),
      CheckIfOverflowed(kToolbarHomeButtonElementId, false),
      CheckIfOverflowed(kToolbarForwardButtonElementId, false),

      // Home button overflows before the forward button. Since we pinned the
      // bookmark action, it will be hidden before the home button, leaving
      // enough space to show the forward button while hiding the home button.
      AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(
          kToolbarHomeButtonElementId),
      // Check that the forward button is still NOT overflowed and is visible.
      CheckIfOverflowed(kToolbarForwardButtonElementId, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, true),
      // Check that the home button is overflowed and hidden.
      CheckIfOverflowed(kToolbarHomeButtonElementId, true),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false),
      // The bookmark bar should also be overflowed.
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                true),

      // Remove the last dummy button.
      Do([&]() {
        std::unique_ptr<views::View> removed_view =
            toolbar_container_view_->RemoveChildViewT(
                toolbar_container_view_->children().back());
      }),
      // That should trigger a new layout. Wait for the home button to become
      // visible again. It should no longer be overflowed.
      //
      // The bookmark button may or may not be overflowed, as it comes with a
      // divider that may or may not cause it to fit with a single dummy button
      // removed, so we don't check it or the overflow button.
      WaitForElementVisibility(kToolbarHomeButtonElementId, true),
      CheckIfOverflowed(kToolbarHomeButtonElementId, false));
}

// Tests that unpinning and pinning the home button while it's overflowed
// correctly shows/hides the forward and overflow buttons.
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest, HomePinUnpinWhileOverflowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

  RunTestSequence(
      InstrumentToolbarWebUiIfNeeded(),
      WaitForElementVisibility(kToolbarHomeButtonElementId, true),
      // Add buttons until the home button overflows. Both home and forward
      // buttons should overflow.
      AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(
          kToolbarHomeButtonElementId),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, false),
      CheckIfOverflowed(kToolbarHomeButtonElementId, true),
      CheckIfOverflowed(kToolbarForwardButtonElementId, true),

      // Unpin the home button. The forward button should become visible,
      // and the overflow button should hide.
      SetBooleanPref(prefs::kShowHomeButton, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, true),
      WaitForHide(kToolbarOverflowButtonElementId),
      CheckIfOverflowed(kToolbarForwardButtonElementId, false),

      // Pin the home button again. Both home and forward buttons should
      // be overflowed and hidden again, and the overflow button should show.
      SetBooleanPref(prefs::kShowHomeButton, true),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, false),
      WaitForShow(kToolbarOverflowButtonElementId),
      CheckIfOverflowed(kToolbarHomeButtonElementId, true),
      CheckIfOverflowed(kToolbarForwardButtonElementId, true));
}

// Tests that unpinning and pinning the forward button while it's overflowed
// correctly shows/hides the home and overflow buttons.
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       ForwardPinUnpinWhileOverflowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton, true);

  RunTestSequence(
      InstrumentToolbarWebUiIfNeeded(),
      WaitForElementVisibility(kToolbarForwardButtonElementId, true),
      // Add buttons until the forward button overflows. Both home and forward
      // buttons should overflow.
      AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(
          kToolbarForwardButtonElementId),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, false),
      CheckIfOverflowed(kToolbarHomeButtonElementId, true),
      CheckIfOverflowed(kToolbarForwardButtonElementId, true),

      // Unpin the forward button. The home button should become visible,
      // and the overflow button should hide.
      SetBooleanPref(prefs::kShowForwardButton, false),
      WaitForElementVisibility(kToolbarHomeButtonElementId, true),
      WaitForHide(kToolbarOverflowButtonElementId),
      CheckIfOverflowed(kToolbarHomeButtonElementId, false),

      // Pin the forward button again. Both home and forward buttons should
      // be overflowed and hidden again, and the overflow button should show.
      SetBooleanPref(prefs::kShowForwardButton, true),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false),
      WaitForElementVisibility(kToolbarForwardButtonElementId, false),
      WaitForShow(kToolbarOverflowButtonElementId),
      CheckIfOverflowed(kToolbarHomeButtonElementId, true),
      CheckIfOverflowed(kToolbarForwardButtonElementId, true));
}

// Tests that pinning the home button when the toolbar is already overflowed
// (and has no space) correctly adds it to the overflow menu and does not
// display it. The main purpose is to make sure that overflow is detected by
// WebUIToolbarWebView when there's no OnBoundsChanged() event.
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest, PinHomeWhileForwardOverflowed) {
  RunTestSequence(
      InstrumentToolbarWebUiIfNeeded(),
      WaitForElementVisibility(kToolbarForwardButtonElementId, true),
      CheckIfOverflowed(kToolbarForwardButtonElementId, false),

      // Add buttons until the forward button overflows.
      AddDummyButtonsToToolbarTillElementOverflowsWithoutResizing(
          kToolbarForwardButtonElementId),
      WaitForElementVisibility(kToolbarForwardButtonElementId, false),
      CheckIfOverflowed(kToolbarForwardButtonElementId, true),
      WaitForShow(kToolbarOverflowButtonElementId),

      // Pin/enable the home button.
      SetBooleanPref(prefs::kShowHomeButton, true),
      // Force a scheduled layout to compute the new overflow state.
      Do([&]() { views::test::RunScheduledLayout(browser_view_); }),
      // Home button should be recognized as overflowed and hidden.
      CheckIfOverflowed(kToolbarHomeButtonElementId, true),
      WaitForElementVisibility(kToolbarHomeButtonElementId, false));
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest, ActivateActionElementFromMenu) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);
  const auto back_url = embedded_test_server()->GetURL("/back");
  const auto forward_url = embedded_test_server()->GetURL("/forward");
  base::UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonActivated"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.MenuItemActivated.ForwardButton"));
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      NavigateWebContents(kPrimaryTabPageElementId, back_url),
      NavigateWebContents(kPrimaryTabPageElementId, forward_url),
      PressButton(kToolbarBackButtonElementId),
      WaitForWebContentsNavigation(kPrimaryTabPageElementId, back_url),
      ForceForwardButtonOverflow(),
      PressButton(kToolbarOverflowButtonElementId),
      ActivateMenuItemWithElementId(kToolbarForwardButtonElementId),

      // Forward navigation is triggered after activating menu item.
      WaitForWebContentsNavigation(kPrimaryTabPageElementId, forward_url));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonActivated"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "ResponsiveToolbar.OverflowMenuItemActivated.ForwardButton"));
}

// TODO(crbug.com/361296257): ActionItemsOverflowAndReappear is flaky on
// linux64-rel-ready.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ActionItemsOverflowAndReappear \
  DISABLED_ActionItemsOverflowAndReappear
#else
#define MAYBE_ActionItemsOverflowAndReappear ActionItemsOverflowAndReappear
#endif
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       MAYBE_ActionItemsOverflowAndReappear) {
  RunTestSequence(PinBookmarkToToolbar(),
                  // Pinned bookmark button is visible.
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, false),

                  AddDummyButtonsToToolbarTillElementOverflows(
                      ChromeActionIds::kActionSidePanelShowBookmarks),

                  // Set browser wider; action item reappears.
                  RestoreBrowserWidth(),
                  WaitForShow(kPinnedToolbarActionsContainerElementId),
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, false));
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       ActionItemsShowInMenuAndActivateFromMenu) {
  RunTestSequence(PinBookmarkToToolbar(),
                  AddDummyButtonsToToolbarTillElementOverflows(
                      ChromeActionIds::kActionSidePanelShowBookmarks),
                  PressButton(kToolbarOverflowButtonElementId),
                  CheckMenuMatchesOverflowedElements(),

                  // Check bookmark menu item is activated correctly.
                  ActivateMenuItemWithElementId(
                      ChromeActionIds::kActionSidePanelShowBookmarks),
                  WaitForShow(kSidePanelElementId), Check([this]() {
                    return browser()
                        ->GetFeatures()
                        .side_panel_ui()
                        ->IsSidePanelEntryShowing(SidePanelEntry::Key(
                            SidePanelEntry::Id::kBookmarks));
                  }));
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       ActivatedActionItemsDoNotOverflow) {
  RunTestSequence(
      PinBookmarkToToolbar(),
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                false),
      EnsureNotPresent(kSidePanelElementId),

      // Open bookmark side panel.
      Do([=, this]() {
        chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_SIDE_PANEL);
      }),
      WaitForShow(kSidePanelElementId), ForceForwardButtonOverflow(),

      // Activated bookmark button is still visible because side panel is open
      // even though it should overflow earlier than forward button.
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                false),

      // Set browser wider; still no overflow.
      RestoreBrowserWidth(),
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                false));
}

// TODO(crbug.com/41495158): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       DISABLED_DeactivatedActionItemsOverflow) {
  RunTestSequence(PinBookmarkToToolbar(),
                  AddDummyButtonsToToolbarTillElementOverflows(
                      ChromeActionIds::kActionSidePanelShowBookmarks),
                  PressButton(kToolbarOverflowButtonElementId),
                  ActivateMenuItemWithElementId(
                      ChromeActionIds::kActionSidePanelShowBookmarks),
                  WaitForShow(kSidePanelElementId),

                  // Close bookmark side panel.
                  PressButton(kSidePanelCloseButtonElementId),
                  WaitForHide(kSidePanelElementId),

                  // Pinned button overflows after side panel is closed.
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, true));
}

IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       EveryElementHasActionMetricName) {
  for (auto& it : ToolbarController::GetDefaultResponsiveElements(browser())) {
    std::visit(
        absl::Overload(
            [](actions::ActionId id) {
              EXPECT_NE(
                  ToolbarController::GetActionNameFromElementIdentifier(id), "")
                  << "Missing metric name for ActionId: " << id;
            },
            [&](ToolbarController::ElementIdInfo id) {
              EXPECT_NE(ToolbarController::GetActionNameFromElementIdentifier(
                            id.activate_identifier),
                        "")
                  << "Missing metric name for ElementIdentifier: "
                  << id.activate_identifier;
            }),
        it.overflow_id);
  }
}

// Verify fixing animation loop bug (crbug.com/).
// Steps to reproduce:
// 1  Set browser with to a big value that nothing should overflow.
// 2. Have 1 pinned extension button in extensions container.
// 3. Have 2 pinned buttons in pinned toolbar container.
// 4. Set browser width to when PinnedToolbarContainer starts to overflow. In
// this case both pinned buttons in PinnedToolbarContainer should overflow,
// overflow button should show. Verify: The pinned extension button should still
// be visible because there's enough space for it. Extensions container should
// not have animation because its visibility didn't change.
// TODO(crbug.com/41495158): Flaky on Windows and Mac.
// TODO(crbug.com/472508632): Test is failing on Linux & CrOS.
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       DISABLED_ExtensionHasNoAnimationLoop) {
  RunTestSequence(
      LoadAndPinExtensionButton(), PinBookmarkToToolbar(),
      PinReadingModeToToolbar(), Do([this]() {
        SetBrowserWidth(GetOverflowThresholdWidthInPinnedSidePanelContainer() -
                        1);
      }),
      WaitForShow(kToolbarOverflowButtonElementId));

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->toolbar()
                   ->extensions_container()
                   ->GetAnimatingLayoutManager()
                   ->is_animating());
}

// TODO(crbug.com/41495158): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DoNotShowIphWhenOverflowed DISABLED_DoNotShowIphWhenOverflowed
#else
#define MAYBE_DoNotShowIphWhenOverflowed DoNotShowIphWhenOverflowed
#endif
IN_PROC_BROWSER_TEST_P(ToolbarControllerUiTest,
                       MAYBE_DoNotShowIphWhenOverflowed) {
  const auto threshold = overflow_threshold_width();
  if (!threshold) {
    GTEST_SKIP();
  }

  RunTestSequence(
      ResizeRelativeToOverflow(-1),
      MaybeShowPromo(feature_engagement::kIPHMemorySaverModeFeature,
                     user_education::FeaturePromoResult::kWindowTooSmall),
      ResizeRelativeToOverflow(+1),
      MaybeShowPromo(feature_engagement::kIPHMemorySaverModeFeature),
      PressClosePromoButton());
}

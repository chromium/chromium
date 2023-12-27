// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include "base/feature_list.h"
#include "base/functional/overloaded.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kBrowserContentAllowedMinimumWidth =
    BrowserViewLayout::kMainBrowserContentsMinimumWidth;
}  // namespace

class ToolbarControllerUiTest : public InteractiveBrowserTest {
 public:
  ToolbarControllerUiTest() {
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
    scoped_feature_list_.InitWithFeatures(
        {features::kResponsiveToolbar, features::kSidePanelPinning,
         features::kChromeRefresh2023},
        {});
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
    InteractiveBrowserTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
    toolbar_controller_ = const_cast<ToolbarController*>(
        browser_view_->toolbar()->toolbar_controller());
    toolbar_container_view_ = const_cast<views::View*>(
        toolbar_controller_->toolbar_container_view_.get());
    overflow_button_ = toolbar_controller_->overflow_button_;
    dummy_button_size_ = overflow_button_->GetPreferredSize();
    responsive_elements_ = toolbar_controller_->responsive_elements_;
    element_flex_order_start_ = toolbar_controller_->element_flex_order_start_;
    MaybeAddDummyButtonsToToolbarView();
    overflow_threshold_width_ = GetOverflowThresholdWidth();
  }

  void TearDownOnMainThread() override {
    toolbar_container_view_ = nullptr;
    overflow_button_ = nullptr;
    toolbar_controller_ = nullptr;
    browser_view_ = nullptr;
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  // Returns the minimum width the toolbar view can be without any elements
  // dropped out.
  int GetOverflowThresholdWidth() {
    int diff_sum = 0;
    for (auto* element : toolbar_container_view_->children()) {
      diff_sum += element->GetPreferredSize().width() -
                  element->GetMinimumSize().width();

      // For containers that can drop out from toolbar and has no elements
      // inside initially (e.g. PinnedToolbarActionsContainer), subtract its
      // margin.
      if (element->GetMinimumSize().width() == 0 &&
          element->GetPreferredSize().width() == 0) {
        diff_sum += GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN);
      }
    }
    return toolbar_container_view_->GetPreferredSize().width() - diff_sum;
  }

  // Because actual_browser_minimum_width == Max(toolbar_width,
  // kBrowserContentAllowedMinimumWidth) So if `overflow_threshold_width_` <
  // kBrowserContentAllowedMinimumWidth, then actual_browser_minimum_width ==
  // kBrowserContentAllowedMinimumWidth In this case we will never see any
  // overflow so stuff toolbar with some fixed dummy buttons till it's
  // guaranteed we can observe overflow with browser resized to its minimum
  // width.
  void MaybeAddDummyButtonsToToolbarView() {
    while (GetOverflowThresholdWidth() <= kBrowserContentAllowedMinimumWidth) {
      toolbar_container_view_->AddChildView(CreateADummyButton());
    }
  }

  std::unique_ptr<ToolbarButton> CreateADummyButton() {
    auto button = std::make_unique<ToolbarButton>();
    button->SetPreferredSize(dummy_button_size_);
    button->SetMinSize(dummy_button_size_);
    button->SetAccessibleName(u"dummybutton");
    button->SetVisible(true);
    return button;
  }

  // Forces `id` to overflow by filling toolbar with dummy buttons.
  void AddDummyButtonsToToolbarTillElementOverflows(
      absl::variant<ui::ElementIdentifier, actions::ActionId> id) {
    // This element must have been managed by controller.
    EXPECT_TRUE(
        std::find_if(
            responsive_elements_.begin(), responsive_elements_.end(),
            [id](const ToolbarController::ResponsiveElementInfo& element) {
              return absl::visit(
                  base::Overloaded(
                      [&](ToolbarController::ElementIdInfo overflow_id) {
                        return absl::holds_alternative<ui::ElementIdentifier>(
                                   id) &&
                               overflow_id.overflow_identifier ==
                                   absl::get<ui::ElementIdentifier>(id);
                      },
                      [&](actions::ActionId overflow_id) {
                        return absl::holds_alternative<actions::ActionId>(id) &&
                               overflow_id == absl::get<actions::ActionId>(id);
                      }),
                  element.overflow_id);
            }) != responsive_elements_.end());

    SetBrowserWidth(kBrowserContentAllowedMinimumWidth);
    absl::visit(
        base::Overloaded{
            [this](ui::ElementIdentifier id) {
              const auto* element =
                  toolbar_controller_->FindToolbarElementWithId(
                      toolbar_container_view_, id);
              ASSERT_NE(element, nullptr);
              while (element->GetVisible()) {
                toolbar_container_view_->AddChildView(CreateADummyButton());
                views::test::RunScheduledLayout(browser_view_);
              }
            },
            [this](actions::ActionId id) {
              while (!delegate()->IsOverflowed(id)) {
                toolbar_container_view_->AddChildView(CreateADummyButton());
                views::test::RunScheduledLayout(browser_view_);
              }
            }},
        id);
  }

  // This checks menu model, not the actual menu that pops up.
  // TODO(pengchaocai): Explore a way to check the actual menu appearing.
  auto CheckMenuMatchesOverflowedElements() {
    return Steps(Check([this]() {
      const ui::SimpleMenuModel* menu = GetOverflowMenu();
      EXPECT_NE(menu, nullptr);
      EXPECT_GT(menu->GetItemCount(), size_t(0));
      const auto& responsive_elements =
          toolbar_controller_->responsive_elements_;
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
      absl::variant<ui::ElementIdentifier, actions::ActionId> id) {
    return Do([=]() {
      int command_id = -1;
      for (size_t i = 0; i < responsive_elements_.size(); ++i) {
        const auto& overflow_id = responsive_elements_[i].overflow_id;
        absl::visit(
            base::Overloaded(
                [&](ToolbarController::ElementIdInfo overflow_id) {
                  if (absl::holds_alternative<ui::ElementIdentifier>(id) &&
                      overflow_id.overflow_identifier ==
                          absl::get<ui::ElementIdentifier>(id)) {
                    command_id = i;
                    return;
                  }
                },
                [&](actions::ActionId overflow_id) {
                  if (absl::holds_alternative<actions::ActionId>(id) &&
                      overflow_id == absl::get<actions::ActionId>(id)) {
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
    return Steps(Do([this]() {
                   AddDummyButtonsToToolbarTillElementOverflows(
                       kToolbarForwardButtonElementId);
                 }),
                 WaitForHide(kToolbarForwardButtonElementId),
                 WaitForShow(kToolbarOverflowButtonElementId));
  }

  auto CheckActionItemOverflowed(actions::ActionId id, bool overflowed) {
    return CheckResult([=]() { return delegate()->IsOverflowed(id); },
                       overflowed);
  }

  auto PinBookmarkToToolbar() {
    return Steps(Do([=]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_BOOKMARK_SIDE_PANEL);
                 }),
                 WaitForShow(kSidePanelElementId), FlushEvents(),
                 PressButton(kSidePanelPinButtonElementId),
                 PressButton(kSidePanelCloseButtonElementId),
                 WaitForHide(kSidePanelElementId), FlushEvents());
  }

  auto SetBrowserSuperWide() {
    return Steps(Do([this]() { SetBrowserWidth(3000); }),
                 WaitForHide(kToolbarOverflowButtonElementId));
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

  gfx::Size dummy_button_size() { return dummy_button_size_; }
  ToolbarController::PinnedActionsDelegate* delegate() {
    return toolbar_controller_->pinned_actions_delegate_;
  }
  const views::View* overflow_button() const { return overflow_button_; }
  int element_flex_order_start() const { return element_flex_order_start_; }
  const std::vector<ToolbarController::ResponsiveElementInfo>&
  responsive_elements() const {
    return responsive_elements_;
  }
  int overflow_threshold_width() const { return overflow_threshold_width_; }
  std::vector<const ToolbarController::ResponsiveElementInfo*>
  GetOverflowedElements() {
    return toolbar_controller_->GetOverflowedElements();
  }
  const ui::SimpleMenuModel* GetOverflowMenu() {
    return static_cast<OverflowButton*>(overflow_button_)
        ->menu_model_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<ToolbarController> toolbar_controller_;
  raw_ptr<views::View> toolbar_container_view_;
  raw_ptr<views::View> overflow_button_;
  std::vector<ToolbarController::ResponsiveElementInfo> responsive_elements_;
  int element_flex_order_start_;
  gfx::Size dummy_button_size_;

  // The minimum width the toolbar view can be without any elements dropped out.
  int overflow_threshold_width_;
};

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       StartBrowserWithThresholdWidth) {
  // Start browser with threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  base::UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonShown"));

  // Resize browser a bit narrower. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonShown"));

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonHidden"));

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ResponsiveToolbar.OverflowButtonHidden"));
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       StartBrowserWithWidthSmallerThanThreshold) {
  // Start browser with a smaller width than threshold. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser wider to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Keep resizing browser narrower. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 2);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should still see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       StartBrowserWithWidthLargerThanThreshold) {
  // Start browser with a larger width than threshold. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser wider. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 2);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest, MenuMatchesOverflowedElements) {
  RunTestSequence(
      Do([this]() { SetBrowserWidth(overflow_threshold_width() - 1); }),
      WaitForShow(kToolbarOverflowButtonElementId),
      PressButton(kToolbarOverflowButtonElementId),
      WaitForActivate(kToolbarOverflowButtonElementId),
      CheckMenuMatchesOverflowedElements());
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest, ActivateActionElementFromMenu) {
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

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       ActionItemsOverflowAndReappear) {
  RunTestSequence(PinBookmarkToToolbar(), SetBrowserSuperWide(),
                  // Pinned bookmark button is visible.
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, false),

                  Do([this]() {
                    AddDummyButtonsToToolbarTillElementOverflows(
                        ChromeActionIds::kActionSidePanelShowBookmarks);
                  }),
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, true),
                  WaitForShow(kToolbarOverflowButtonElementId),

                  // Set browser super wide action item reappears.
                  SetBrowserSuperWide(),
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, false),
                  WaitForHide(kToolbarOverflowButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       ActionItemsShowInMenuAndActivateFromMenu) {
  RunTestSequence(
      PinBookmarkToToolbar(), SetBrowserSuperWide(), Do([this]() {
        AddDummyButtonsToToolbarTillElementOverflows(
            ChromeActionIds::kActionSidePanelShowBookmarks);
      }),
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                true),
      WaitForShow(kToolbarOverflowButtonElementId),
      PressButton(kToolbarOverflowButtonElementId),
      CheckMenuMatchesOverflowedElements(),

      // Check bookmark menu item is activated correctly.
      ActivateMenuItemWithElementId(
          ChromeActionIds::kActionSidePanelShowBookmarks),
      WaitForShow(kSidePanelElementId), FlushEvents(), Check([this]() {
        auto* coordinator =
            SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
        return coordinator->IsSidePanelEntryShowing(
            SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
      }));
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       ActivatedActionItemsDoNotOverflow) {
  RunTestSequence(
      PinBookmarkToToolbar(), SetBrowserSuperWide(),
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                false),
      EnsureNotPresent(kSidePanelElementId),

      // Open bookmark side panel.
      Do([=]() {
        chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_SIDE_PANEL);
      }),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      ForceForwardButtonOverflow(),

      // Activated bookmark button is still visible because side panel is open
      // even though it should overflow earlier than forward button.
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                false),

      // Set browser wide still no overflow.
      SetBrowserSuperWide(),
      CheckActionItemOverflowed(ChromeActionIds::kActionSidePanelShowBookmarks,
                                false));
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       DeactivatedActionItemsOverflow) {
  RunTestSequence(PinBookmarkToToolbar(), SetBrowserSuperWide(), Do([this]() {
                    AddDummyButtonsToToolbarTillElementOverflows(
                        ChromeActionIds::kActionSidePanelShowBookmarks);
                  }),
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, true),
                  WaitForShow(kToolbarOverflowButtonElementId),
                  PressButton(kToolbarOverflowButtonElementId),
                  ActivateMenuItemWithElementId(
                      ChromeActionIds::kActionSidePanelShowBookmarks),
                  WaitForShow(kSidePanelElementId), FlushEvents(),

                  // Close bookmark side panel.
                  PressButton(kSidePanelCloseButtonElementId),
                  WaitForHide(kSidePanelElementId), FlushEvents(),

                  // Pinned button overflows after side panel is closed.
                  CheckActionItemOverflowed(
                      ChromeActionIds::kActionSidePanelShowBookmarks, true));
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       EveryElementHasActionMetricName) {
  for (auto& it : ToolbarController::GetDefaultResponsiveElements(browser())) {
    absl::visit(
        base::Overloaded(
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

class ToolbarControllerIphUiTest : public ToolbarControllerUiTest {
 public:
  ToolbarControllerIphUiTest() {
    iph_feature_list_.InitForDemo(
        feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
  }
  ~ToolbarControllerIphUiTest() override = default;

  auto TryShowHelpBubble(user_education::FeaturePromoResult expected_result =
                             user_education::FeaturePromoResult::Success()) {
    std::ostringstream desc;
    desc << "TryShowHelpBubble(" << expected_result << ")";
    return CheckResult(
        [this]() {
          return browser()->window()->MaybeShowFeaturePromo(
              feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
        },
        expected_result, desc.str());
  }

  auto DismissHelpBubble() {
    auto result = Steps(
        PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
    AddDescription(result, "DismissHelpBubble( %s )");
    return result;
  }

  auto ResizeRelativeToOverflow(int diff) {
    return Do(
        [this, diff]() { SetBrowserWidth(overflow_threshold_width() + diff); });
  }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ToolbarControllerIphUiTest, DoNotShowIphWhenOverflowed) {
  RunTestSequence(
      ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true),
      ResizeRelativeToOverflow(-1),
      TryShowHelpBubble(user_education::FeaturePromoResult::kBlockedByUi),
      ResizeRelativeToOverflow(1), TryShowHelpBubble(), DismissHelpBubble());
}

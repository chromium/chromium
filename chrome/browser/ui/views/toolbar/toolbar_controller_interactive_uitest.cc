// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <sstream>

#include "base/functional/overloaded.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kBrowserContentAllowedMinimumWidth =
    BrowserViewLayout::kMainBrowserContentsMinimumWidth;
}  // namespace

class ToolbarControllerUiTest : public InteractiveFeaturePromoTest {
 public:
  ToolbarControllerUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHTabSearchFeature})) {
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, false);
    views::test::WaitForAnimatingLayoutManager(
        browser_view_->toolbar()->pinned_toolbar_actions_container());
    toolbar_controller_ = const_cast<ToolbarController*>(
        browser_view_->toolbar()->toolbar_controller());
    toolbar_container_view_ = const_cast<views::View*>(
        toolbar_controller_->toolbar_container_view_.get());
    overflow_button_ = toolbar_controller_->overflow_button_;
    dummy_button_size_ = overflow_button_->GetPreferredSize();
    responsive_elements_ = toolbar_controller_->responsive_elements_;
    element_flex_order_start_ = toolbar_controller_->element_flex_order_start_;
    MaybeAddDummyButtonsToToolbarView();
    overflow_threshold_width_ = GetOverflowThresholdWidthInToolbarContainer();
    default_browser_width_ = browser()->window()->GetBounds().width();
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
    ToolbarController::ElementIdInfo d;
    return Check(
        [this, id]() {
          for (const auto& el : responsive_elements_) {
            if (const auto* info =
                    absl::get_if<ToolbarController::ElementIdInfo>(
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
          for (const auto& el : responsive_elements_) {
            if (const auto* action =
                    absl::get_if<actions::ActionId>(&el.overflow_id)) {
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
                                          overflowed ? "true" : "false"));
  }

  // Forces `id` to overflow by filling toolbar with dummy buttons.
  auto AddDummyButtonsToToolbarTillElementOverflows(ui::ElementIdentifier id) {
    auto result =
        Steps(CheckIsManagedByController(id),
              std::move(Do([this]() {
                          SetBrowserWidth(kBrowserContentAllowedMinimumWidth);
                        }).SetDescription("SetBrowserWidth()")),
              std::move(Do([this, id]() {
                          const auto* element =
                              toolbar_controller_->FindToolbarElementWithId(
                                  toolbar_container_view_, id);
                          ASSERT_NE(element, nullptr);
                          while (element->GetVisible()) {
                            toolbar_container_view_->AddChildView(
                                CreateADummyButton());
                            views::test::RunScheduledLayout(browser_view_);
                          }
                        }).SetDescription("ForceOverflow")),
              WaitForShow(kToolbarOverflowButtonElementId), WaitForHide(id));
    AddDescription(result,
                   "AddDummyButtonsToToolbarTillElementOverflows( %s )");
    return result;
  }

  auto AddDummyButtonsToToolbarTillElementOverflows(actions::ActionId id) {
    auto result =
        Steps(CheckIsManagedByController(id),
              std::move(Do([this]() {
                          SetBrowserWidth(kBrowserContentAllowedMinimumWidth);
                        }).SetDescription("SetBrowserWidth()")),
              std::move(Do([this, id]() {
                          while (!delegate()->IsOverflowed(id)) {
                            toolbar_container_view_->AddChildView(
                                CreateADummyButton());
                            views::test::RunScheduledLayout(browser_view_);
                          }
                        }).SetDescription("ForceOverflow")),
              WaitForShow(kToolbarOverflowButtonElementId),
              CheckActionItemOverflowed(id, true));
    AddDescription(result,
                   "AddDummyButtonsToToolbarTillElementOverflows( %s )");
    return result;
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
    return Do([=, this]() {
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
    return Do(
        [this, diff]() { SetBrowserWidth(overflow_threshold_width() + diff); });
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
    return toolbar_controller_->menu_model_for_testing();
  }
  BrowserView* browser_view() { return browser_view_.get(); }

 private:
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<ToolbarController> toolbar_controller_;
  raw_ptr<views::View> toolbar_container_view_;
  raw_ptr<views::View> overflow_button_;
  std::vector<ToolbarController::ResponsiveElementInfo> responsive_elements_;
  int element_flex_order_start_;
  gfx::Size dummy_button_size_;

  // The minimum width the toolbar view can be without any elements dropped out.
  int overflow_threshold_width_;

  int default_browser_width_;
};

// TODO(crbug.com/41495158): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_StartBrowserWithThresholdWidth \
  DISABLED_StartBrowserWithThresholdWidth
#else
#define MAYBE_StartBrowserWithThresholdWidth StartBrowserWithThresholdWidth
#endif
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       MAYBE_StartBrowserWithThresholdWidth) {
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
// TODO(crbug.com/41495158): Flaky on Windows.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StartBrowserWithWidthSmallerThanThreshold \
  DISABLED_StartBrowserWithWidthSmallerThanThreshold
#else
#define MAYBE_StartBrowserWithWidthSmallerThanThreshold \
  StartBrowserWithWidthSmallerThanThreshold
#endif
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       MAYBE_StartBrowserWithWidthSmallerThanThreshold) {
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

// TODO(crbug.com/360465388): Lacros failures are because resize doesn't
// actually stick.
// TODO(crbug/361296257): ActionItemsOverflowAndReappear is flaky on
// linux64-rel-ready.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ActionItemsOverflowAndReappear \
  DISABLED_ActionItemsOverflowAndReappear
#else
#define MAYBE_ActionItemsOverflowAndReappear ActionItemsOverflowAndReappear
#endif
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
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

// TODO(crbug.com/360465388): Lacros failures are because resize doesn't
// actually stick.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ActionItemsShowInMenuAndActivateFromMenu \
  DISABLED_ActionItemsShowInMenuAndActivateFromMenu
#else
#define MAYBE_ActionItemsShowInMenuAndActivateFromMenu \
  ActionItemsShowInMenuAndActivateFromMenu
#endif
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       MAYBE_ActionItemsShowInMenuAndActivateFromMenu) {
  RunTestSequence(
      PinBookmarkToToolbar(),
      AddDummyButtonsToToolbarTillElementOverflows(
          ChromeActionIds::kActionSidePanelShowBookmarks),
      PressButton(kToolbarOverflowButtonElementId),
      CheckMenuMatchesOverflowedElements(),

      // Check bookmark menu item is activated correctly.
      ActivateMenuItemWithElementId(
          ChromeActionIds::kActionSidePanelShowBookmarks),
      WaitForShow(kSidePanelElementId), Check([this]() {
        auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
        return coordinator->IsSidePanelEntryShowing(
            SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
      }));
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
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
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ExtensionHasNoAnimationLoop DISABLED_ExtensionHasNoAnimationLoop
#else
#define MAYBE_ExtensionHasNoAnimationLoop ExtensionHasNoAnimationLoop
#endif
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       MAYBE_ExtensionHasNoAnimationLoop) {
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

// TODO(crbug.com/41495158): Flaky on Windows and fails on Lacros.
// TODO(crbug.com/360465388): Lacros failures are because resize doesn't
// actually stick.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DoNotShowIphWhenOverflowed DISABLED_DoNotShowIphWhenOverflowed
#else
#define MAYBE_DoNotShowIphWhenOverflowed DoNotShowIphWhenOverflowed
#endif
IN_PROC_BROWSER_TEST_F(ToolbarControllerUiTest,
                       MAYBE_DoNotShowIphWhenOverflowed) {
  RunTestSequence(
      ResizeRelativeToOverflow(-1),
      MaybeShowPromo(feature_engagement::kIPHTabSearchFeature,
                     user_education::FeaturePromoResult::kBlockedByUi),
      ResizeRelativeToOverflow(1),
      MaybeShowPromo(feature_engagement::kIPHTabSearchFeature),
      PressClosePromoButton());
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/events.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

namespace {
constexpr char16_t kBubbleBodyText[] = u"Bubble body text.";
constexpr char16_t kBubbleButtonText[] = u"Button";
constexpr char16_t kCloseButtonAltText[] = u"Close";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadLaterWebContentsElementId);
}  // namespace

class HelpBubbleFactoryWebUIInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleFactoryWebUIInteractiveUiTest() = default;
  ~HelpBubbleFactoryWebUIInteractiveUiTest() override = default;

  // Opens the side panel and instruments the Read Later WebContents as
  // kReadLaterWebContentsElementId.
  auto OpenReadingListSidePanel() {
    if (features::IsChromeRefresh2023() &&
        base::FeatureList::IsEnabled(features::kSidePanelPinning)) {
      return Steps(
          PressButton(kToolbarAppMenuButtonElementId),
          SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
          SelectMenuItem(BookmarkSubMenuModel::kReadingListMenuItem),
          SelectMenuItem(ReadingListSubMenuModel::kReadingListMenuShowUI),
          WaitForShow(kSidePanelElementId),
          WaitForShow(kReadLaterSidePanelWebViewElementId), FlushEvents(),
          // Ensure that the Reading List side panel loads properly.
          InstrumentNonTabWebView(kReadLaterWebContentsElementId,
                                  kReadLaterSidePanelWebViewElementId));
    }

    return Steps(
        // Remove delays in switching side panels to prevent possible race
        // conditions when selecting items from the side panel dropdown.
        Do([this]() {
          SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser())
              ->SetNoDelaysForTesting(true);
        }),
        // Click the Side Panel button and wait for the side panel to appear.
        PressButton(kToolbarSidePanelButtonElementId),
        WaitForShow(kSidePanelElementId), FlushEvents(),
        // Select the Reading List side panel and wait for the WebView to
        // appear.
        SelectDropdownItem(kSidePanelComboboxElementId,
                           static_cast<int>(SidePanelEntry::Id::kReadingList)),
        WaitForShow(kReadLaterSidePanelWebViewElementId),
        // Ensure that the Reading List side panel loads properly.
        InstrumentNonTabWebView(kReadLaterWebContentsElementId,
                                kReadLaterSidePanelWebViewElementId));
  }

  auto OpenBookmarksSidePanel() {
    if (features::IsChromeRefresh2023() &&
        base::FeatureList::IsEnabled(features::kSidePanelPinning)) {
      return Steps(
          PressButton(kToolbarAppMenuButtonElementId),
          SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
          SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkSidePanelItem),
          WaitForShow(kSidePanelElementId), FlushEvents());
    }

    // Yes, this assumes the side panel is already open.
    return Steps(
        EnsurePresent(kSidePanelElementId),
        SelectDropdownItem(kSidePanelComboboxElementId,
                           static_cast<int>(SidePanelEntry::Id::kBookmarks)));
  }

  auto ShowHelpBubble(ElementSpecifier element) {
    return InAnyContext(std::move(
        AfterShow(
            element,
            base::BindLambdaForTesting(
                [this](ui::InteractionSequence* seq, ui::TrackedElement* el) {
                  help_bubble_ = GetHelpBubbleFactory()->CreateHelpBubble(
                      el, GetHelpBubbleParams());
                  if (!help_bubble_) {
                    LOG(ERROR) << "Failed to create help bubble.";
                    seq->FailForTesting();
                  }
                }))
            .SetDescription("ShowHelpBubble")));
  }

  auto CloseHelpBubble() {
    return Do(base::BindLambdaForTesting([this]() { help_bubble_->Close(); }));
  }

  auto CheckHandlerHasHelpBubble(ElementSpecifier anchor,
                                 bool has_help_bubble) {
    return InAnyContext(
        std::move(CheckElement(
                      anchor,
                      [](ui::TrackedElement* el) {
                        return el->AsA<user_education::TrackedElementWebUI>()
                            ->handler()
                            ->IsHelpBubbleShowingForTesting(el->identifier());
                      },
                      has_help_bubble)
                      .SetDescription(base::StringPrintf(
                          "CheckHandlerHasHelpBubble(%s)",
                          has_help_bubble ? "true" : "false"))));
  }

 protected:
  std::unique_ptr<user_education::HelpBubble> help_bubble_;

 private:
  static user_education::HelpBubbleParams GetHelpBubbleParams() {
    user_education::HelpBubbleButtonParams button_params;
    button_params.is_default = true;
    button_params.text = kBubbleButtonText;
    button_params.callback = base::DoNothing();
    user_education::HelpBubbleParams params;
    params.body_text = kBubbleBodyText;
    params.close_button_alt_text = kCloseButtonAltText;
    params.buttons.emplace_back(std::move(button_params));
    return params;
  }

  user_education::HelpBubbleFactoryRegistry* GetHelpBubbleFactory() {
    auto* const controller = browser()->window()->GetFeaturePromoController();
    return static_cast<BrowserFeaturePromoController*>(controller)
        ->bubble_factory_registry();
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       ShowFloatingHelpBubble) {
  const DeepQuery kPathToAddCurrentTabElement{"reading-list-app",
                                              "#currentPageActionButton"};
  RunTestSequence(
      OpenReadingListSidePanel(),
      ShowHelpBubble(kAddCurrentTabToReadingListElementId),

      // Verify that the handler does not believe a WebUI help bubble is
      // present.
      CheckHandlerHasHelpBubble(kAddCurrentTabToReadingListElementId, false),

      // Verify that the anchor element is marked.
      CheckJsResultAt(
          kReadLaterWebContentsElementId, kPathToAddCurrentTabElement,
          "el => el.classList.contains('help-anchor-highlight')", true),

      // Expect the help bubble to display with the correct parameters.
      CheckViewProperty(
          user_education::HelpBubbleView::kDefaultButtonIdForTesting,
          &views::LabelButton::GetText, kBubbleButtonText),

      // Expect the bubble to overlap the side panel slightly, as the anchor
      // element is not flush with the edge of the side panel.
      CheckView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                base::BindOnce(
                    [](ui::ElementContext context, views::View* bubble) {
                      const gfx::Rect bubble_rect =
                          bubble->GetWidget()->GetWindowBoundsInScreen();
                      const gfx::Rect side_panel_rect =
                          views::ElementTrackerViews::GetInstance()
                              ->GetFirstMatchingView(kSidePanelElementId,
                                                     context)
                              ->GetBoundsInScreen();
                      return bubble_rect.Intersects(side_panel_rect);
                    },
                    browser()->window()->GetElementContext())),

      CloseHelpBubble(),

      // Verify that the anchor element is no longer marked.
      CheckJsResultAt(
          kReadLaterWebContentsElementId, kPathToAddCurrentTabElement,
          "el => el.classList.contains('help-anchor-highlight')", false));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       ShowEmbeddedHelpBubbleAndCloseViaExternalApi) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
  static const DeepQuery kPathToHelpBubbleBody = {"user-education-internals",
                                                  "#IPH_WebUiHelpBubbleTest",
                                                  "help-bubble", "#topBody"};
  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId,
                          GURL("chrome://internals/user-education")),
      ShowHelpBubble(kWebUIIPHDemoElementIdentifier),

      // Verify that the handler believes that the anchor has a help bubble.
      CheckHandlerHasHelpBubble(kWebUIIPHDemoElementIdentifier, true),

      CheckJsResultAt(kBrowserTabId, kPathToHelpBubbleBody,
                      "el => el.innerText", base::UTF16ToUTF8(kBubbleBodyText)),
      CloseHelpBubble(),

      // Verify that the handler no longer believes that the anchor has a help
      // bubble.
      CheckHandlerHasHelpBubble(kWebUIIPHDemoElementIdentifier, false));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       ShowEmbeddedHelpBubbleAndCloseViaClick) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHelpBubbleHiddenEvent);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
  static const DeepQuery kPathToHelpBubbleCloseButton = {
      "user-education-internals", "#IPH_WebUiHelpBubbleTest", "help-bubble",
      "#close"};
  StateChange bubble_hidden;
  bubble_hidden.type = StateChange::Type::kDoesNotExist;
  bubble_hidden.where = kPathToHelpBubbleCloseButton;
  bubble_hidden.event = kHelpBubbleHiddenEvent;

  RunTestSequence(
      InstrumentTab(kBrowserTabId),
      NavigateWebContents(kBrowserTabId,
                          GURL("chrome://internals/user-education")),
      ShowHelpBubble(kWebUIIPHDemoElementIdentifier),

      ExecuteJsAt(kBrowserTabId, kPathToHelpBubbleCloseButton,
                  "el => el.click()"),
      WaitForStateChange(kBrowserTabId, bubble_hidden), FlushEvents(),

      // Verify that the handler no longer believes that the anchor has a help
      // bubble.
      CheckHandlerHasHelpBubble(kWebUIIPHDemoElementIdentifier, false));
}

// Regression test for item (1) in crbug.com/1422875.
IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       FloatingHelpBubbleHiddenOnWebUiHidden) {
  RunTestSequence(
      OpenReadingListSidePanel(),
      ShowHelpBubble(kAddCurrentTabToReadingListElementId),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Switch to a different side panel; this removes the Reading List WebView
      // from its widget and effectively hides the WebContents.
      OpenBookmarksSidePanel(),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

namespace {
constexpr char kSidePanelElementName[] = "Side Panel Element";
}

class HelpBubbleFactoryRtlWebUIInteractiveUiTest
    : public HelpBubbleFactoryWebUIInteractiveUiTest {
 public:
  HelpBubbleFactoryRtlWebUIInteractiveUiTest() = default;
  ~HelpBubbleFactoryRtlWebUIInteractiveUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                    switches::kForceDirectionRTL);
  }
};

// This verifies that the "element bounds updated" event gets sent when the side
// panel is resized, even if none of the elements in the side panel are resized.
// This is a regression test for crbug.com/1425487.
IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryRtlWebUIInteractiveUiTest,
                       ResizeSidePanelSendsUpdate) {
  RunTestSequence(
      OpenReadingListSidePanel(),
      InAnyContext(
          AfterShow(kAddCurrentTabToReadingListElementId,
                    [](ui::InteractionSequence* seq, ui::TrackedElement* el) {
                      seq->NameElement(el, kSidePanelElementName);
                    })),
      ShowHelpBubble(kSidePanelElementName), FlushEvents(),
      WithView(kSidePanelElementId,
               [](SidePanel* side_panel) {
                 side_panel->OnResize(-50, true);
                 side_panel->GetWidget()->LayoutRootViewIfNecessary();
               }),
      WaitForEvent(kSidePanelElementName,
                   user_education::kHelpBubbleAnchorBoundsChangedEvent));
}

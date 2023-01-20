// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/widget_focus_waiter.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/webui/floating_webui_help_bubble_factory.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

namespace {
constexpr char16_t kBubbleBodyText[] = u"Bubble body text.";
constexpr char16_t kBubbleButtonText[] = u"Button";
constexpr char16_t kCloseButtonAltText[] = u"Close";
}  // namespace

class HelpBubbleFactoryWebUIInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleFactoryWebUIInteractiveUiTest() = default;
  ~HelpBubbleFactoryWebUIInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    // TODO(crbug.com/1399570): this is a workaround for a side search bug that
    // can randomly close the side panel.
    auto* config = SideSearchConfig::Get(browser()->profile());
    config->set_skip_on_template_url_changed_for_testing(true);
  }

  auto ShowHelpBubble(ElementSpecifier element) {
    return InAnyContext(
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
            .SetDescription("ShowHelpBubble"));
  }

  auto CloseHelpBubble() {
    return Do(base::BindLambdaForTesting([this]() { help_bubble_->Close(); }));
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
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadLaterElementId);
  const DeepQuery kPathToAddCurrentTabElement{"reading-list-app",
                                              "#currentPageActionButton"};
  RunTestSequence(
      // Click on the toolbar button to show the side panel.
      PressButton(kSidePanelButtonElementId), WaitForShow(kSidePanelElementId),
      FlushEvents(),
      SelectDropdownItem(kSidePanelComboboxElementId,
                         static_cast<int>(SidePanelEntry::Id::kReadingList)),
      ShowHelpBubble(kAddCurrentTabToReadingListElementId),

      // Verify that the anchor element is marked.
      InstrumentNonTabWebView(kReadLaterElementId,
                              kReadLaterSidePanelWebViewElementId),
      CheckJsResultAt(kReadLaterElementId, kPathToAddCurrentTabElement,
                      "el => el.classList.contains('help-anchor-highlight')",
                      true),

      // Expect the help bubble to display with the correct parameters.
      CheckView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                base::BindOnce([](user_education::HelpBubbleView* bubble) {
                  return bubble->GetDefaultButtonForTesting()->GetText() ==
                         kBubbleButtonText;
                })),

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

      // Verify that the anchor element is no longer marked.
      CloseHelpBubble(),
      CheckJsResultAt(kReadLaterElementId, kPathToAddCurrentTabElement,
                      "el => el.classList.contains('help-anchor-highlight')",
                      false));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryWebUIInteractiveUiTest,
                       ShowEmbeddedHelpBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
  static const DeepQuery kPathToHelpBubbleBody = {"user-education-internals",
                                                  "help-bubble", "#topBody"};
  RunTestSequence(InstrumentTab(kBrowserTabId),
                  NavigateWebContents(
                      kBrowserTabId, GURL("chrome://internals/user-education")),
                  ShowHelpBubble(kWebUIIPHDemoElementIdentifier),
                  CheckJsResultAt(kBrowserTabId, kPathToHelpBubbleBody,
                                  "(el) => el.innerText",
                                  base::UTF16ToUTF8(kBubbleBodyText)));
}

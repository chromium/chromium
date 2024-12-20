// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/user_education/common/anchor_element_provider.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/common_preconditions.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {
class TestAnchorProvider : public user_education::AnchorElementProviderCommon {
 public:
  TestAnchorProvider(ui::ElementIdentifier id, bool in_any_context)
      : AnchorElementProviderCommon(id) {
    set_in_any_context(in_any_context);
  }
};
}  // namespace

class BrowserFeaturePromoPreconditionsUiTest : public InteractiveBrowserTest {
 public:
  BrowserFeaturePromoPreconditionsUiTest() = default;
  ~BrowserFeaturePromoPreconditionsUiTest() override = default;

  std::unique_ptr<user_education::AnchorElementPrecondition>
  CreateAnchorPrecondition(
      ui::ElementIdentifier id,
      bool in_any_context,
      ui::ElementContext default_context = ui::ElementContext()) {
    if (!default_context) {
      default_context = browser()->window()->GetElementContext();
    }
    CHECK(!anchor_element_provider_ ||
          (anchor_element_provider_->anchor_element_id() == id &&
           anchor_element_provider_->in_any_context() == in_any_context));
    if (!anchor_element_provider_) {
      anchor_element_provider_ =
          std::make_unique<TestAnchorProvider>(id, in_any_context);
    }
    return std::make_unique<user_education::AnchorElementPrecondition>(
        *anchor_element_provider_, default_context);
  }

 private:
  std::unique_ptr<TestAnchorProvider> anchor_element_provider_;
};

using WindowActivePreconditionUiTest = BrowserFeaturePromoPreconditionsUiTest;

IN_PROC_BROWSER_TEST_F(WindowActivePreconditionUiTest, ElementInActiveBrowser) {
  RunTestSequence(
      WaitForShow(kToolbarAppMenuButtonElementId),
      CheckResult(
          [this]() {
            const auto anchor_precond =
                CreateAnchorPrecondition(kToolbarAppMenuButtonElementId,
                                         /*in_any_context=*/false);
            WindowActivePrecondition active_precond;
            user_education::FeaturePromoPrecondition::ComputedData data;
            EXPECT_TRUE(anchor_precond->CheckPrecondition(data));
            return active_precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(WindowActivePreconditionUiTest,
                       ElementInInactiveBrowser) {
  auto* const incog = CreateIncognitoBrowser();
  RunTestSequence(
      WaitForShow(kToolbarAppMenuButtonElementId),
      InContext(incog->window()->GetElementContext(),
                WaitForShow(kToolbarAppMenuButtonElementId)),
      CheckResult(
          [this, incog]() {
            // On different platforms, activation of the second window can occur
            // differently. So instead of trying to guess which will be active,
            // explicitly find the inactive one.
            Browser* inactive = incog->IsActive() ? browser() : incog;
            EXPECT_FALSE(inactive->IsActive());
            const auto anchor_precond = CreateAnchorPrecondition(
                kToolbarAppMenuButtonElementId,
                /*in_any_context=*/false,
                inactive->window()->GetElementContext());
            WindowActivePrecondition active_precond;
            user_education::FeaturePromoPrecondition::ComputedData data;
            EXPECT_TRUE(anchor_precond->CheckPrecondition(data));
            return active_precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::kBlockedByUi));
}

IN_PROC_BROWSER_TEST_F(WindowActivePreconditionUiTest, PageInActiveTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  RunTestSequence(
      InstrumentTab(kTabId),
      NavigateWebContents(kTabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)),
      CheckResult(
          [this]() {
            const auto anchor_precond =
                CreateAnchorPrecondition(kWebUIIPHDemoElementIdentifier,
                                         /*in_any_context=*/true);
            WindowActivePrecondition active_precond;
            user_education::FeaturePromoPrecondition::ComputedData data;
            EXPECT_TRUE(anchor_precond->CheckPrecondition(data));
            return active_precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::Success()));
}

IN_PROC_BROWSER_TEST_F(WindowActivePreconditionUiTest, PageInInactiveTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId2);
  RunTestSequence(
      InstrumentTab(kTabId1),
      AddInstrumentedTab(kTabId2,
                         GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)),

      // Switch away from the tab. It is no longer "active".
      SelectTab(kTabStripElementId, 0),
      CheckResult(
          [this]() {
            const auto anchor_precond =
                CreateAnchorPrecondition(kWebUIIPHDemoElementIdentifier,
                                         /*in_any_context=*/true);
            WindowActivePrecondition active_precond;
            user_education::FeaturePromoPrecondition::ComputedData data;
            // When the tab is not the active tab, the element isn't even
            // present.
            EXPECT_FALSE(anchor_precond->CheckPrecondition(data));
            return active_precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::kBlockedByUi),

      // Switch back to the tab and verify that it is "active" again.
      SelectTab(kTabStripElementId, 1),
      InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)),
      CheckResult(
          [this]() {
            const auto anchor_precond =
                CreateAnchorPrecondition(kWebUIIPHDemoElementIdentifier,
                                         /*in_any_context=*/true);
            WindowActivePrecondition active_precond;
            user_education::FeaturePromoPrecondition::ComputedData data;
            // When the tab is not the active tab, the element isn't even
            // present.
            EXPECT_TRUE(anchor_precond->CheckPrecondition(data));
            return active_precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::Success()));
}

using OmniboxNotOpenPreconditionUiTest = BrowserFeaturePromoPreconditionsUiTest;

IN_PROC_BROWSER_TEST_F(OmniboxNotOpenPreconditionUiTest,
                       CheckOmniboxClosedAndOpen) {
  RunTestSequence(
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            OmniboxNotOpenPrecondition precond(*browser_view);
            user_education::FeaturePromoPrecondition::ComputedData data;
            return precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::Success()),
      WithView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            AutocompleteInput input(
                u"chrome", metrics::OmniboxEventProto::NTP,
                ChromeAutocompleteSchemeClassifier(browser_view->GetProfile()));
            browser_view->GetLocationBarView()
                ->GetOmniboxView()
                ->controller()
                ->autocomplete_controller()
                ->Start(input);
          }),
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            OmniboxNotOpenPrecondition precond(*browser_view);
            user_education::FeaturePromoPrecondition::ComputedData data;
            return precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::kBlockedByUi));
}

class ToolbarNotCollapsedPreconditionUiTest
    : public BrowserFeaturePromoPreconditionsUiTest {
 public:
  ToolbarNotCollapsedPreconditionUiTest() {
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
  }
  ~ToolbarNotCollapsedPreconditionUiTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ToolbarNotCollapsedPreconditionUiTest,
                       ToolbarExpandedAndCollapsed) {
  RunTestSequence(
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            ToolbarNotCollapsedPrecondition precond(*browser_view);
            user_education::FeaturePromoPrecondition::ComputedData data;
            return precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::Success()),

      // Add elements to the toolbar until something collapses.
      WithView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            const ToolbarController* const controller =
                browser_view->toolbar()->toolbar_controller();
            CHECK(controller);
            auto* const forward_button =
                views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                    kToolbarForwardButtonElementId,
                    browser_view->GetElementContext());
            auto* const container_view =
                views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                    ToolbarView::kToolbarContainerElementId,
                    browser_view->GetElementContext());
            constexpr gfx::Size kButtonSize{16, 16};
            while (forward_button->GetVisible()) {
              auto* const button = container_view->AddChildView(
                  std::make_unique<ToolbarButton>());
              button->SetPreferredSize(kButtonSize);
              button->SetMinSize(kButtonSize);
              button->GetViewAccessibility().SetName(u"dummy");
              button->SetVisible(true);
              views::test::RunScheduledLayout(browser_view);
            }
          }),
      WaitForShow(kToolbarOverflowButtonElementId),

      CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            ToolbarNotCollapsedPrecondition precond(*browser_view);
            user_education::FeaturePromoPrecondition::ComputedData data;
            return precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::kBlockedByUi));
}

using BrowserNotClosingPreconditionUiTest =
    BrowserFeaturePromoPreconditionsUiTest;

IN_PROC_BROWSER_TEST_F(BrowserNotClosingPreconditionUiTest,
                       BrowserClosingOrNotClosing) {
  RunTestSequence(
      WaitForShow(kBrowserViewElementId),
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            BrowserNotClosingPrecondition precond(*browser_view);
            user_education::FeaturePromoPrecondition::ComputedData data;
            return precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::Success()),
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            BrowserNotClosingPrecondition precond(*browser_view);
            user_education::FeaturePromoPrecondition::ComputedData data;
            browser_view->GetWidget()->Close();
            return precond.CheckPrecondition(data);
          },
          user_education::FeaturePromoResult::kBlockedByUi)
          .SetMustRemainVisible(false));
}

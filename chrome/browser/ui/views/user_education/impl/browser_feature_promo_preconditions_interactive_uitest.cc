// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/anchor_element_provider.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/common_preconditions.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
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

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoPreconditionsUiTest,
                       WindowActivePrecondition_ElementInActiveBrowser) {
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

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoPreconditionsUiTest,
                       WindowActivePrecondition_ElementInInactiveBrowser) {
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

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoPreconditionsUiTest,
                       WindowActivePrecondition_PageInActiveTab) {
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

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoPreconditionsUiTest,
                       WindowActivePrecondition_PageInInactiveTab) {
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

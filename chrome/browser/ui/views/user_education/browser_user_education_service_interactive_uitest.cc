// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_result.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMainContentsElementId);
constexpr char kToolbarName[] = "Toolbar";
constexpr char kContentsPaneName[] = "Contents Pane";
constexpr char kGetActiveElementJs[] = R"(
  function() {
    let el = document.activeElement;
    while (el && el.shadowRoot && el.shadowRoot.activeElement) {
      el = el.shadowRoot.activeElement;
    }
    return el ? el.id : '';
  }
)";
}  // namespace

class BrowserUserEducationServiceUiTest : public InteractiveFeaturePromoTest {
 public:
  BrowserUserEducationServiceUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHWebUiHelpBubbleTestFeature})) {}
  ~BrowserUserEducationServiceUiTest() override = default;

  UserEducationService* GetUserEducationService() {
    return UserEducationServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  auto DoSetup() {
    return Steps(InstrumentTab(kMainContentsElementId),
                 NavigateWebContents(kMainContentsElementId,
                                     GURL("chrome://internals/user-education")),
                 NameDescendantViewByType<ToolbarView>(kTopContainerElementId,
                                                       kToolbarName),
                 NameViewRelative(kBrowserViewElementId, kContentsPaneName,
                                  [](BrowserView* browser_view) {
                                    return browser_view->contents_web_view();
                                  }),
                 InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)));
  }

  auto EnsureFocus(ElementSpecifier spec, bool focused) {
    return CheckViewProperty(spec, &views::View::HasFocus, focused);
  }

  auto FocusAppMenuButton() {
    return Steps(WithView(kToolbarName,
                          [](ToolbarView* toolbar) {
                            toolbar->SetPaneFocusAndFocusAppMenu();
                          }),
                 EnsureFocus(kToolbarAppMenuButtonElementId, true));
  }
};

IN_PROC_BROWSER_TEST_F(BrowserUserEducationServiceUiTest,
                       WebUIHelpBubbleTakesFocus) {
  RunTestSequence(
      DoSetup(), FocusAppMenuButton(), EnsureFocus(kContentsPaneName, false),
      MaybeShowPromo(feature_engagement::kIPHWebUiHelpBubbleTestFeature,
                     WebUiHelpBubbleShown()),
      EnsureFocus(kContentsPaneName, true),
      CheckJsResult(kMainContentsElementId, kGetActiveElementJs,
                    "action-button-1"));
}

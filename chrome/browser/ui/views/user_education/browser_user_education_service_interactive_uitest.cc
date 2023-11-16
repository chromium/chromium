// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
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

class BrowserUserEducationServiceUiTest : public InteractiveBrowserTest {
 public:
  BrowserUserEducationServiceUiTest() = default;
  ~BrowserUserEducationServiceUiTest() override = default;

  UserEducationService* GetUserEducationService() {
    return UserEducationServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  void SetUp() override {
    feature_list_.InitForDemo(
        feature_engagement::kIPHWebUiHelpBubbleTestFeature);
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  auto DoSetup() {
    return Steps(ObserveState(kFeatureEngagementInitializedState, browser()),
                 InstrumentTab(kMainContentsElementId),
                 NavigateWebContents(kMainContentsElementId,
                                     GURL("chrome://internals/user-education")),
                 NameDescendantViewByType<ToolbarView>(kTopContainerElementId,
                                                       kToolbarName),
                 NameViewRelative(kBrowserViewElementId, kContentsPaneName,
                                  [](BrowserView* browser_view) {
                                    return browser_view->contents_web_view();
                                  }),
                 InAnyContext(WaitForShow(kWebUIIPHDemoElementIdentifier)),
                 WaitForState(kFeatureEngagementInitializedState, true));
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

  auto ShowHelpBubble() {
    return CheckResult(
        [this]() {
          return browser()->window()->MaybeShowFeaturePromo(
              feature_engagement::kIPHWebUiHelpBubbleTestFeature);
        },
        user_education::FeaturePromoResult::Success(), "ShowHelpBubble()");
  }

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserUserEducationServiceUiTest,
                       WebUIHelpBubbleTakesFocus) {
  RunTestSequence(DoSetup(), FocusAppMenuButton(),
                  EnsureFocus(kContentsPaneName, false), ShowHelpBubble(),
                  EnsureFocus(kContentsPaneName, true),
                  CheckJsResult(kMainContentsElementId, kGetActiveElementJs,
                                "action-button-1"));
}

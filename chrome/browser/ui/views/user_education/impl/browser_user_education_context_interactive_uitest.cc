// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"

#include <functional>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/display/screen.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowser1TabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowser2TabId);
constexpr std::string_view kElement1Name = "element1";
constexpr std::string_view kElement2Name = "element2";
}  // namespace

class BrowserUserEducationContextUiTest : public InteractiveBrowserTest {
 public:
  BrowserUserEducationContextUiTest() = default;
  ~BrowserUserEducationContextUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
    browser_test_impl().set_max_dom_nodes(100);
  }
};

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest, OneProfileFindsView) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  const auto ui_context = BrowserElements::From(browser())->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();

  auto elements = tracker->GetAllMatchingElements(
      kToolbarAppMenuButtonElementId, ui_context);
  ASSERT_FALSE(elements.empty());
  auto* expected = tracker->GetFirstMatchingElement(
      kToolbarAppMenuButtonElementId, ui_context);
  ASSERT_NE(nullptr, expected);
  EXPECT_EQ(expected, filter.Run(elements));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       OneProfileFindsWebUiAnchor) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const tracker = ui::ElementTracker::GetElementTracker();

  RunTestSequence(
      InstrumentTab(kBrowser1TabId),
      NavigateWebContents(kBrowser1TabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(WaitForShow(UserEducationInternalsUI::kToolbarElementId)));

  auto elements = tracker->GetAllMatchingElementsInAnyContext(
      UserEducationInternalsUI::kToolbarElementId);
  ASSERT_FALSE(elements.empty());
  auto* expected = tracker->GetElementInAnyContext(
      UserEducationInternalsUI::kToolbarElementId);
  ASSERT_NE(nullptr, expected);
  EXPECT_EQ(expected, filter.Run(elements));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       TwoProfilesFindsView) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  const auto ui_context = BrowserElements::From(browser())->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());

  RunTestSequenceInContext(
      BrowserElements::From(incognito_browser)->GetContext(),
      WaitForShow(kToolbarAppMenuButtonElementId));

  auto elements = tracker->GetAllMatchingElementsInAnyContext(
      kToolbarAppMenuButtonElementId);
  ASSERT_EQ(2U, elements.size());
  auto* expected = tracker->GetFirstMatchingElement(
      kToolbarAppMenuButtonElementId, ui_context);
  ASSERT_NE(nullptr, expected);
  EXPECT_EQ(expected, filter.Run(elements));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       TwoProfilesDoesNotFindView) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  const auto ui_context = BrowserElements::From(browser())->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());

  RunTestSequence(
      InContext(BrowserElements::From(incognito_browser)->GetContext(),
                WaitForShow(kToolbarAppMenuButtonElementId)),
      // Hide the app menu button in the original browser.
      WithView(kToolbarAppMenuButtonElementId,
               [](views::View* view) { view->SetVisible(false); }),
      WaitForHide(kToolbarAppMenuButtonElementId));

  auto elements = tracker->GetAllMatchingElementsInAnyContext(
      kToolbarAppMenuButtonElementId);
  ASSERT_EQ(1U, elements.size());
  auto* expected = tracker->GetFirstMatchingElement(
      kToolbarAppMenuButtonElementId, ui_context);
  ASSERT_EQ(nullptr, expected);
  EXPECT_EQ(expected, filter.Run(elements));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       TwoProfilesFindsWebUiAnchor) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());

  ui::TrackedElement* expected = nullptr;

  RunTestSequence(
      InstrumentTab(kBrowser1TabId),
      NavigateWebContents(kBrowser1TabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(
          AfterShow(UserEducationInternalsUI::kToolbarElementId,
                    [&expected](ui::TrackedElement* el) { expected = el; })),
      InContext(BrowserElements::From(incognito_browser)->GetContext(),
                WaitForShow(kToolbarAppMenuButtonElementId),
                InstrumentTab(kBrowser2TabId),
                InParallel(
                    RunSubsequence(NavigateWebContents(
                        kBrowser2TabId,
                        GURL(chrome::kChromeUIUserEducationInternalsURL))),
                    RunSubsequence(InAnyContext(
                        WaitForShow(UserEducationInternalsUI::kToolbarElementId)
                            .SetTransitionOnlyOnEvent(true))))));

  auto elements = tracker->GetAllMatchingElementsInAnyContext(
      UserEducationInternalsUI::kToolbarElementId);
  ASSERT_EQ(2U, elements.size());
  EXPECT_EQ(expected, filter.Run(elements));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       TwoProfilesDoesNotFindWebUiAnchor) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());

  RunTestSequenceInContext(
      BrowserElements::From(incognito_browser)->GetContext(),
      WaitForShow(kToolbarAppMenuButtonElementId),
      InstrumentTab(kBrowser2TabId),
      NavigateWebContents(kBrowser2TabId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      InAnyContext(WaitForShow(UserEducationInternalsUI::kToolbarElementId)));

  auto elements = tracker->GetAllMatchingElementsInAnyContext(
      UserEducationInternalsUI::kToolbarElementId);
  ASSERT_EQ(1U, elements.size());
  EXPECT_EQ(nullptr, filter.Run(elements));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       PrefersViewInOriginalBrowser) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const other = CreateBrowser(browser()->GetProfile());
  const auto ui_context1 = BrowserElements::From(browser())->GetContext();
  const auto ui_context2 = BrowserElements::From(other)->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  ui::TrackedElement* element1;
  ui::TrackedElement* element2;

  RunTestSequence(
      InContext(
          ui_context1,
          AfterShow(kToolbarAppMenuButtonElementId,
                    [&element1](ui::TrackedElement* el) { element1 = el; }),
          NameElement(kElement1Name, std::ref(element1))),
      InContext(
          ui_context2,
          AfterShow(kToolbarAppMenuButtonElementId,
                    [&element2](ui::TrackedElement* el) { element2 = el; }),
          NameElement(kElement2Name, std::ref(element2))),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kSkipTest,
          "Some platforms do not support direct window activation."),
      ActivateSurface(kElement1Name),
      CheckElement(kElement1Name,
                   [&](ui::TrackedElement* expected) {
                     ui::TrackedElement* actual =
                         filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                             kToolbarAppMenuButtonElementId));
                     if (actual != expected) {
                       LOG(ERROR)
                           << "Expected " << *expected << " actual " << *actual;
                       return false;
                     }
                     return true;
                   }),
      ActivateSurface(kElement2Name),
      CheckElement(kElement1Name, [&](ui::TrackedElement* expected) {
        ui::TrackedElement* actual =
            filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                kToolbarAppMenuButtonElementId));
        if (actual != expected) {
          LOG(ERROR) << "Expected " << *expected << " actual " << *actual;
          return false;
        }
        return true;
      }));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       TracksViewInActiveBrowser) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const other = CreateBrowser(browser()->GetProfile());
  auto* const other2 = CreateBrowser(browser()->GetProfile());
  const auto ui_context1 = BrowserElements::From(other)->GetContext();
  const auto ui_context2 = BrowserElements::From(other2)->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  ui::TrackedElement* element1;
  ui::TrackedElement* element2;

  RunTestSequence(
      WaitForShow(kToolbarAppMenuButtonElementId),
      WithView(kToolbarAppMenuButtonElementId,
               [](views::View* view) { view->SetVisible(false); }),
      WaitForHide(kToolbarAppMenuButtonElementId),
      InContext(
          ui_context1,
          AfterShow(kToolbarAppMenuButtonElementId,
                    [&element1](ui::TrackedElement* el) { element1 = el; }),
          NameElement(kElement1Name, std::ref(element1))),
      InContext(
          ui_context2,
          AfterShow(kToolbarAppMenuButtonElementId,
                    [&element2](ui::TrackedElement* el) { element2 = el; }),
          NameElement(kElement2Name, std::ref(element2))),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kSkipTest,
          "Some platforms do not support direct window activation."),
      ActivateSurface(kElement1Name),
      CheckElement(kElement1Name,
                   [&](ui::TrackedElement* expected) {
                     ui::TrackedElement* actual =
                         filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                             kToolbarAppMenuButtonElementId));
                     if (actual != expected) {
                       LOG(ERROR)
                           << "Expected " << *expected << " actual " << *actual;
                       return false;
                     }
                     return true;
                   }),
      ActivateSurface(kElement2Name),
      CheckElement(kElement2Name, [&](ui::TrackedElement* expected) {
        ui::TrackedElement* actual =
            filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                kToolbarAppMenuButtonElementId));
        if (actual != expected) {
          LOG(ERROR) << "Expected " << *expected << " actual " << *actual;
          return false;
        }
        return true;
      }));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       PrefersWebUiAnchorInOriginalBrowser) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const other = CreateBrowser(browser()->GetProfile());
  const auto ui_context1 = BrowserElements::From(browser())->GetContext();
  const auto ui_context2 = BrowserElements::From(other)->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  ui::TrackedElement* element1;
  ui::TrackedElement* element2;

  RunTestSequence(
      InContext(ui_context1, InstrumentTab(kBrowser1TabId),
                NavigateWebContents(
                    kBrowser1TabId,
                    GURL(chrome::kChromeUIUserEducationInternalsURL))),
      InAnyContext(
          AfterShow(UserEducationInternalsUI::kToolbarElementId,
                    [&element1](ui::TrackedElement* el) { element1 = el; })),
      NameElement(kElement1Name, std::ref(element1)),
      InContext(ui_context2, WaitForShow(kToolbarAppMenuButtonElementId),
                InstrumentTab(kBrowser2TabId)),
      InParallel(
          RunSubsequence(NavigateWebContents(
              kBrowser2TabId,
              GURL(chrome::kChromeUIUserEducationInternalsURL))),
          RunSubsequence(InAnyContext(
              AfterShow(UserEducationInternalsUI::kToolbarElementId,
                        [&element2](ui::TrackedElement* el) { element2 = el; })
                  .SetTransitionOnlyOnEvent(true)))),
      NameElement(kElement2Name, std::ref(element2)),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kSkipTest,
          "Some platforms do not support direct window activation."),
      ActivateSurface(kElement1Name),
      CheckElement(kElement1Name,
                   [&](ui::TrackedElement* expected) {
                     ui::TrackedElement* actual =
                         filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                             UserEducationInternalsUI::kToolbarElementId));
                     if (actual != expected) {
                       LOG(ERROR)
                           << "Expected " << *expected << " actual " << *actual;
                       return false;
                     }
                     return true;
                   }),
      ActivateSurface(kElement2Name),
      CheckElement(kElement1Name, [&](ui::TrackedElement* expected) {
        ui::TrackedElement* actual =
            filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                UserEducationInternalsUI::kToolbarElementId));
        if (actual != expected) {
          LOG(ERROR) << "Expected " << *expected << " actual " << *actual;
          return false;
        }
        return true;
      }));
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationContextUiTest,
                       PrefersWebUiAnchorInActiveBrowser) {
  auto ue_context = BrowserUserEducationInterface::From(browser())
                        ->GetUserEducationContextForTesting();
  auto filter = ue_context->GetDefaultElementFilter();
  auto* const other = CreateBrowser(browser()->GetProfile());
  auto* const other2 = CreateBrowser(browser()->GetProfile());
  const auto ui_context1 = BrowserElements::From(other)->GetContext();
  const auto ui_context2 = BrowserElements::From(other2)->GetContext();
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  ui::TrackedElement* element1;
  ui::TrackedElement* element2;

  RunTestSequence(
      InContext(ui_context1, WaitForShow(kToolbarAppMenuButtonElementId),
                InstrumentTab(kBrowser1TabId),
                NavigateWebContents(
                    kBrowser1TabId,
                    GURL(chrome::kChromeUIUserEducationInternalsURL))),
      InAnyContext(
          AfterShow(UserEducationInternalsUI::kToolbarElementId,
                    [&element1](ui::TrackedElement* el) { element1 = el; })),
      NameElement(kElement1Name, std::ref(element1)),
      InContext(ui_context2, WaitForShow(kToolbarAppMenuButtonElementId),
                InstrumentTab(kBrowser2TabId)),
      InParallel(
          RunSubsequence(NavigateWebContents(
              kBrowser2TabId,
              GURL(chrome::kChromeUIUserEducationInternalsURL))),
          RunSubsequence(InAnyContext(
              AfterShow(UserEducationInternalsUI::kToolbarElementId,
                        [&element2](ui::TrackedElement* el) { element2 = el; })
                  .SetTransitionOnlyOnEvent(true)))),
      NameElement(kElement2Name, std::ref(element2)),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kSkipTest,
          "Some platforms do not support direct window activation."),
      ActivateSurface(kElement1Name),
      CheckElement(kElement1Name,
                   [&](ui::TrackedElement* expected) {
                     ui::TrackedElement* actual =
                         filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                             UserEducationInternalsUI::kToolbarElementId));
                     if (actual != expected) {
                       LOG(ERROR)
                           << "Expected " << *expected << " actual " << *actual;
                       return false;
                     }
                     return true;
                   }),
      ActivateSurface(kElement2Name),
      CheckElement(kElement2Name, [&](ui::TrackedElement* expected) {
        ui::TrackedElement* actual =
            filter.Run(tracker->GetAllMatchingElementsInAnyContext(
                UserEducationInternalsUI::kToolbarElementId));
        if (actual != expected) {
          LOG(ERROR) << "Expected " << *expected << " actual " << *actual;
          return false;
        }
        return true;
      }));
}

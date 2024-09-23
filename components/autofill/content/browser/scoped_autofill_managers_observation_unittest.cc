// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/scoped_autofill_managers_observation.h"

#include "base/test/gtest_util.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Ref;

class ScopedAutofillManagersObservationTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

 protected:
  // TODO(crbug.com/40276395): Move this code (and the nearly identical function
  // in `FormForest`'s unittest) into a common helper function.
  content::RenderFrameHost* CreateChildFrameAndNavigate(
      content::RenderFrameHost* parent_frame,
      const GURL& url,
      const std::string& name) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHostTester::For(parent_frame)->AppendChild(name);
    std::unique_ptr<content::NavigationSimulator> simulator;
    // In unit tests, two navigations are needed for
    // `ContentAutofillDriverFactory` to trigger driver creation.
    GURL about_blank("about:blank");
    CHECK_NE(about_blank, url);
    simulator =
        content::NavigationSimulator::CreateRendererInitiated(about_blank, rfh);
    simulator->Commit();
    rfh = simulator->GetFinalRenderFrameHost();
    simulator = content::NavigationSimulator::CreateRendererInitiated(url, rfh);
    simulator->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
    simulator->Commit();
    return simulator->GetFinalRenderFrameHost();
  }

  AutofillManager* manager(content::RenderFrameHost* rfh) {
    ContentAutofillDriver* driver = autofill_driver_injector_[rfh];
    return driver ? &driver->GetAutofillManager() : nullptr;
  }

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<ContentAutofillDriver> autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
};

TEST_F(ScopedAutofillManagersObservationTest, SingleFrameObservation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(web_contents());
  NavigateAndCommit(GURL("https://a.com/"));

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(main_rfh()))));
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithDelayedInitializationFailsWithStrictPolicy) {
  NavigateAndCommit(GURL("https://a.com/"));
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);

  EXPECT_CHECK_DEATH(observation.Observe(
      web_contents(), ScopedAutofillManagersObservation::InitializationPolicy::
                          kExpectNoPreexistingManagers));
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithDelayedInitialization) {
  NavigateAndCommit(GURL("https://a.com/"));

  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(web_contents(),
                      ScopedAutofillManagersObservation::InitializationPolicy::
                          kObservePreexistingManagers);

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(main_rfh()))));
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       SingleFrameObservationWithNavigation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(web_contents());
  NavigateAndCommit(GURL("https://a.com/"));

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(main_rfh()))));
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);

  NavigateAndCommit(GURL("https://b.com/"));
  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(main_rfh()))));
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest, NoObservationsAfterReset) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(web_contents());
  NavigateAndCommit(GURL("https://a.com/"));

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(main_rfh()))));
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);

  observation.Reset();
  EXPECT_CALL(observer, OnBeforeLanguageDetermined).Times(0);
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest, MultipleFrameObservation) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(web_contents());
  NavigateAndCommit(GURL("https://a.com/"));

  content::RenderFrameHost* child_rfh =
      CreateChildFrameAndNavigate(main_rfh(), GURL("https://b.com/"), "child");

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(main_rfh()))));
  manager(main_rfh())
      ->NotifyObservers(&AutofillManager::Observer::OnBeforeLanguageDetermined);

  ASSERT_TRUE(manager(child_rfh));
  EXPECT_CALL(observer, OnBeforeLanguageDetermined(Ref(*manager(child_rfh))));
  manager(child_rfh)->NotifyObservers(
      &AutofillManager::Observer::OnBeforeLanguageDetermined);
}

TEST_F(ScopedAutofillManagersObservationTest,
       StateChangedToPendingDeletionNotifiesObserver) {
  MockAutofillManagerObserver observer;
  ScopedAutofillManagersObservation observation(&observer);
  observation.Observe(web_contents());
  NavigateAndCommit(GURL("https://a.com/"));

  EXPECT_CALL(observer, OnAutofillManagerStateChanged(
                            Ref(*manager(main_rfh())), _,
                            AutofillDriver::LifecycleState::kPendingDeletion));
  ContentAutofillDriverFactory::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());
}

}  // namespace autofill

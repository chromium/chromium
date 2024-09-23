// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/test_matchers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

namespace autofill {

namespace {

using ::autofill::test::LazyRef;
using ::autofill::test::SaveArgPtr;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Between;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Truly;

auto HasState(AutofillDriver::LifecycleState expected_state) {
  return Property("AutofillDriver::GetLifecycleState",
                  &AutofillDriver::GetLifecycleState, expected_state);
}

class MockContentAutofillDriverFactoryObserver
    : public ContentAutofillDriverFactory::Observer {
 public:
  MOCK_METHOD(void,
              OnContentAutofillDriverFactoryDestroyed,
              (ContentAutofillDriverFactory & factory),
              (override));
  MOCK_METHOD(void,
              OnContentAutofillDriverCreated,
              (ContentAutofillDriverFactory & factory,
               ContentAutofillDriver& driver),
              (override));
  MOCK_METHOD(void,
              OnContentAutofillDriverStateChanged,
              (ContentAutofillDriverFactory & factory,
               ContentAutofillDriver& driver,
               AutofillDriver::LifecycleState old_state,
               AutofillDriver::LifecycleState new_state),
              (override));
};

// Test case with one frame.
class ContentAutofillDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  TestContentAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  ContentAutofillDriverFactory& factory() {
    return client()->GetAutofillDriverFactory();
  }

  void NavigateMainFrame(std::string_view url) {
    content::NavigationSimulator::CreateBrowserInitiated(GURL(url),
                                                         web_contents())
        ->Commit();
  }

  ContentAutofillDriver& driver(content::RenderFrameHost* rfh = nullptr) {
    return CHECK_DEREF(
        test_api(factory()).DriverForFrame(rfh ? rfh : main_frame()));
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

 protected:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

TEST_F(ContentAutofillDriverFactoryTest, MainDriver) {
  NavigateMainFrame("https://a.com/");
  ContentAutofillDriver* main_driver =
      test_api(factory()).GetDriver(main_rfh());
  EXPECT_TRUE(main_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);
  EXPECT_EQ(ContentAutofillDriver::GetForRenderFrameHost(main_rfh()),
            main_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);
  EXPECT_EQ(ContentAutofillDriver::GetForRenderFrameHost(main_rfh()),
            main_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);
}

enum class SourceOfRecursion {
  kOnAutofillDriverCreated,
  kOnAutofillDriverStateChanged
};

// The parameter specifies where the recursive events come from.
class ContentAutofillDriverFactoryTest_Recursion
    : public ContentAutofillDriverFactoryTest,
      public ::testing::WithParamInterface<SourceOfRecursion> {
 public:
  SourceOfRecursion source_of_recursion() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    ContentAutofillDriverFactoryTest,
    ContentAutofillDriverFactoryTest_Recursion,
    testing::Values(SourceOfRecursion::kOnAutofillDriverCreated,
                    SourceOfRecursion::kOnAutofillDriverStateChanged));

// Tests that DriverForFrame() tolerates recursive calls. These recursive calls
// come from ContentAutofillDriverFactory::Observer events.
TEST_P(ContentAutofillDriverFactoryTest_Recursion, RecursiveDriverForFrame) {
  // Creates 100 frames without associated drivers.
  NavigateMainFrame("https://a.com/");
  std::set<content::RenderFrameHost*> frames;
  std::set<ContentAutofillDriver*> drivers;
  while (frames.size() < 100) {
    frames.insert(
        content::RenderFrameHostTester::For(main_rfh())->AppendChild("child"));
  }

  // Creates all the drivers in recursive DriverForFrame() calls.
  auto create_drivers = [&](ContentAutofillDriverFactory& factory,
                            ContentAutofillDriver& driver, auto...) {
    for (auto& rfh : frames) {
      drivers.insert(test_api(factory).DriverForFrame(rfh));
    }
  };
  MockContentAutofillDriverFactoryObserver observer;
  factory().AddObserver(&observer);
  switch (source_of_recursion()) {
    case SourceOfRecursion::kOnAutofillDriverCreated:
      EXPECT_CALL(observer, OnContentAutofillDriverCreated)
          .WillRepeatedly(create_drivers);
      break;
    case SourceOfRecursion::kOnAutofillDriverStateChanged:
      EXPECT_CALL(observer, OnContentAutofillDriverStateChanged)
          .WillRepeatedly(create_drivers);
      break;
  }

  // Triggers the recursion.
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);
  EXPECT_EQ(frames.size(), 100u);
  EXPECT_EQ(drivers.size(), 0u);
  test_api(factory()).DriverForFrame(*frames.begin());
  EXPECT_EQ(frames.size(), 100u);
  EXPECT_EQ(drivers.size(), 100u);
  EXPECT_EQ(test_api(factory()).num_drivers(), 101u);
  factory().RemoveObserver(&observer);

  // Validate that the drivers are still registered.
  EXPECT_THAT(
      frames, Each(Truly([&](content::RenderFrameHost* rfh) {
        return drivers.contains(test_api(factory()).DriverForFrame(rfh));
      })));
}

// Test case with two frames: the main frame and one child frame.
class ContentAutofillDriverFactoryTest_WithTwoFrames
    : public ContentAutofillDriverFactoryTest {
 public:
  void NavigateChildFrame(std::string_view url) {
    CHECK(main_rfh());
    if (!child_rfh()) {
      child_rfh_id_ = content::RenderFrameHostTester::For(main_rfh())
                          ->AppendChild(std::string("child"))
                          ->GetGlobalId();
    }
    child_rfh_id_ = content::NavigationSimulator::NavigateAndCommitFromDocument(
                        GURL(url), child_rfh())
                        ->GetGlobalId();
  }

  content::RenderFrameHost* child_rfh() {
    return content::RenderFrameHost::FromID(child_rfh_id_);
  }

 private:
  content::GlobalRenderFrameHostId child_rfh_id_;
};

TEST_F(ContentAutofillDriverFactoryTest_WithTwoFrames, TwoDrivers) {
  NavigateMainFrame("https://a.com/");
  NavigateChildFrame("https://b.com/");
  ASSERT_TRUE(main_rfh());
  ASSERT_TRUE(child_rfh());
  ContentAutofillDriver* main_driver =
      ContentAutofillDriver::GetForRenderFrameHost(main_rfh());
  ContentAutofillDriver* child_driver =
      ContentAutofillDriver::GetForRenderFrameHost(child_rfh());
  EXPECT_TRUE(main_driver);
  EXPECT_TRUE(child_driver);
  EXPECT_EQ(ContentAutofillDriver::GetForRenderFrameHost(main_rfh()),
            main_driver);
  EXPECT_EQ(ContentAutofillDriver::GetForRenderFrameHost(child_rfh()),
            child_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 2u);
  EXPECT_EQ(ContentAutofillDriver::GetForRenderFrameHost(main_rfh()),
            main_driver);
  EXPECT_EQ(ContentAutofillDriver::GetForRenderFrameHost(child_rfh()),
            child_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 2u);
}

// Test case with two frames, where the parameter selects one of them.
class ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne
    : public ContentAutofillDriverFactoryTest_WithTwoFrames,
      public ::testing::WithParamInterface<bool> {
 public:
  content::RenderFrameHost* picked_rfh() {
    return GetParam() ? main_rfh() : child_rfh();
  }
};

INSTANTIATE_TEST_SUITE_P(ContentAutofillDriverFactoryTest,
                         ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne,
                         testing::Bool());

// Tests that a driver is removed in RenderFrameDeleted().
TEST_P(ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne,
       RenderFrameDeleted) {
  NavigateMainFrame("https://a.com/");
  NavigateChildFrame("https://b.com/");
  ASSERT_TRUE(picked_rfh() == main_rfh() || picked_rfh() == child_rfh());
  ContentAutofillDriver* main_driver =
      ContentAutofillDriver::GetForRenderFrameHost(main_rfh());
  ContentAutofillDriver* child_driver =
      ContentAutofillDriver::GetForRenderFrameHost(child_rfh());
  EXPECT_TRUE(main_driver);
  EXPECT_TRUE(child_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 2u);
  factory().RenderFrameDeleted(picked_rfh());
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);
  if (picked_rfh() == main_rfh()) {
    EXPECT_EQ(test_api(factory()).GetDriver(child_rfh()), child_driver);
  } else {
    EXPECT_EQ(test_api(factory()).GetDriver(main_rfh()), main_driver);
  }
}

// Test case with one frame, with BFcache enabled or disabled depending on the
// parameter.
class ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes
    : public ContentAutofillDriverFactoryTest,
      public ::testing::WithParamInterface<std::tuple<bool>> {
 public:
  ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes() {
    std::vector<base::test::FeatureRef> enabled;
    // Allow BackForwardCache for all devices regardless of their memory.
    std::vector<base::test::FeatureRef> disabled =
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting();
    (use_bfcache() ? enabled : disabled)
        .push_back(::features::kBackForwardCache);
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  bool use_bfcache() { return std::get<0>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ContentAutofillDriverFactoryTest,
    ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
    testing::Combine(testing::Bool()));

// Tests that that a same-documentation navigation does not touch the factory's
// router.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       SameDocumentNavigation) {
  NavigateMainFrame("https://a.com/");
  content::RenderFrameHost* orig_rfh = main_rfh();
  ContentAutofillDriver* orig_driver =
      ContentAutofillDriver::GetForRenderFrameHost(orig_rfh);
  NavigateMainFrame("https://a.com/#same-site");
  ASSERT_EQ(orig_rfh, main_rfh());
  EXPECT_EQ(test_api(factory()).GetDriver(orig_rfh), orig_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);

  // TODO(crbug.com/40178290): Test that |router_| has been untouched. To this
  // end, call `orig_driver->FormsSeen({FormData{}})` above and then check
  // here that the router still knows that form. For this to work, we need mock
  // AutofillManagers instead of real BrowserAutofillManager, which are blocked
  // by ContentAutofillDriver's use of the factory callback.
}

// Tests that that a driver is 1:1 with RenderFrameHost, which might or might
// not change after a same-origin navigation.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       SameOriginNavigation) {
  NavigateMainFrame("https://a.com/");
  content::RenderFrameHost* orig_rfh = main_rfh();
  ContentAutofillDriver* orig_driver =
      ContentAutofillDriver::GetForRenderFrameHost(orig_rfh);

  // TODO(crbug.com/40178290): Use mock AutofillManagers and expect a call of
  // AutofillManager::Reset(), which is blocked by ContentAutofillDriver's use
  // of the factory callback.

  NavigateMainFrame("https://a.com/after-navigation");
  // If the RenderFrameHost changed, a new driver for main_rfh() is created, and
  // if BFCache is disabled the driver for |orig_rfh| has now been removed in
  // ContentAutofillDriverFactory::RenderFrameDeleted().
  if (use_bfcache()) {
    EXPECT_EQ(test_api(factory()).GetDriver(orig_rfh), orig_driver);
  } else if (main_rfh() != orig_rfh) {
    EXPECT_EQ(test_api(factory()).GetDriver(orig_rfh), nullptr);
  }
  EXPECT_EQ(test_api(factory()).num_drivers(), use_bfcache() ? 2u : 1u);
}

// Tests that that a driver is removed and replaced with a fresh one after a
// cross-origin navigation.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       CrossOriginNavigation) {
  NavigateMainFrame("https://a.com/");
  content::RenderFrameHost* orig_rfh = main_rfh();
  content::GlobalRenderFrameHostId orig_rfh_id = orig_rfh->GetGlobalId();
  ContentAutofillDriver* orig_driver =
      ContentAutofillDriver::GetForRenderFrameHost(orig_rfh);

  ASSERT_EQ(orig_rfh, main_rfh());
  EXPECT_EQ(test_api(factory()).GetDriver(orig_rfh), orig_driver);
  EXPECT_EQ(test_api(factory()).num_drivers(), 1u);

  NavigateMainFrame("https://different-origin-after-navigation.com/");

  ASSERT_NE(orig_rfh_id, main_rfh()->GetGlobalId());
  // A new driver for main_rfh() has been created and the |orig_rfh| has now
  // been removed in ContentAutofillDriverFactory::RenderFrameDeleted(), unless
  // BFcache is enabled (or main_rfh() happens to have the same address as
  // |orig_rfh|).
  if (use_bfcache()) {
    EXPECT_EQ(test_api(factory()).GetDriver(orig_rfh), orig_driver);
  } else if (main_rfh() != orig_rfh) {
    EXPECT_EQ(test_api(factory()).GetDriver(orig_rfh), nullptr);
  }
  EXPECT_NE(test_api(factory()).GetDriver(main_rfh()), nullptr);
  EXPECT_EQ(test_api(factory()).num_drivers(), use_bfcache() ? 2u : 1u);
}

// Fixture for testing that Autofill is enabled in fenced frames.
class ContentAutofillDriverFactoryTest_FencedFrames
    : public ContentAutofillDriverFactoryTest {
 public:
  ContentAutofillDriverFactoryTest_FencedFrames() {
    std::vector<base::test::FeatureRefAndParams> enabled{
        {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
        {blink::features::kFencedFramesAPIChanges, {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled, {});
  }

  ~ContentAutofillDriverFactoryTest_FencedFrames() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContentAutofillDriverFactoryTest_FencedFrames,
       DisableAutofillWithinFencedFrame) {
  NavigateMainFrame("http://test.org");
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  content::RenderFrameHost* fenced_frame_subframe =
      content::RenderFrameHostTester::For(fenced_frame_root)
          ->AppendChild("iframe");
  EXPECT_NE(nullptr, ContentAutofillDriver::GetForRenderFrameHost(main_rfh()));
  EXPECT_NE(nullptr,
            ContentAutofillDriver::GetForRenderFrameHost(fenced_frame_root));
  EXPECT_NE(nullptr, ContentAutofillDriver::GetForRenderFrameHost(
                         fenced_frame_subframe));
}

// Test fixture for the LifecycleState changes of ContentAutofillDriver.
class ContentAutofillDriverFactoryTestLifecycleState
    : public ContentAutofillDriverFactoryTest {
 public:
  enum class NavigationType {
    // Same-document navigations do not affect the CAD's lifecycle.
    kSameDocument,
    // Same-origin navigations. If the the previous RFH is reused, will cause
    // a CAD::Reset().
    kSameOrigin,
    // Cross-origin navigations. The navigations will swap the RFH, leading to
    // a new CAD.
    kCrossOrigin,
    // Backward navigations are served by BFCache and re-activate a cached CAD.
    kBackward,
    // Prerendering navigations create but do not activate a new CAD.
    kPrerender,
    // Prerendering activations activate a CAD that was created after a
    // kPrerender navigation.
    kPrerenderedActivation,
  };

  using LifecycleState = AutofillDriver::LifecycleState;
  using enum LifecycleState;

  class FactoryObserver : public ContentAutofillDriverFactory::Observer {
   public:
    MOCK_METHOD(void,
                OnContentAutofillDriverFactoryDestroyed,
                (ContentAutofillDriverFactory & factory),
                (override));
    MOCK_METHOD(void,
                OnContentAutofillDriverCreated,
                (ContentAutofillDriverFactory & factory,
                 ContentAutofillDriver& driver),
                (override));
    MOCK_METHOD(void,
                OnContentAutofillDriverStateChanged,
                (ContentAutofillDriverFactory & factory,
                 ContentAutofillDriver& driver,
                 LifecycleState old_state,
                 LifecycleState new_state),
                (override));
  };

  void SetUp() override {
    ContentAutofillDriverFactoryTest::SetUp();
    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL("https://a.test/"));
    web_contents_delegate_.emplace(*web_contents());
    factory().AddObserver(&factory_observer_);
  }

  void TearDown() override {
    if (client()) {
      factory().RemoveObserver(&factory_observer_);
    }
    web_contents_delegate_.reset();
    ContentAutofillDriverFactoryTest::TearDown();
  }

  FactoryObserver& observer() { return factory_observer_; }

  content::RenderFrameHost* Navigate(NavigationType type) {
    // This must "a.test" (or "http").
    // Otherwise AddPrerenderAndCommitNavigation() hits a DCHECK.
    const GURL kPrerenderUrl = GURL("https://a.test/prerender");
    const GURL kNonPrerenderUrl = GURL("https://a.test/navigated");

    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(kNonPrerenderUrl,
                                                              main_frame());
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    switch (type) {
      case NavigationType::kSameDocument: {
        content::RenderFrameHost* rfh1 = main_frame();
        simulator->CommitSameDocument();
        content::RenderFrameHost* rfh2 = simulator->GetFinalRenderFrameHost();
        CHECK_EQ(rfh1, rfh2);
        return rfh2;
      }

      case NavigationType::kSameOrigin: {
        content::RenderFrameHost* rfh1 = main_frame();
        content::RenderFrameHost* rfh2 = simulator->Reload(web_contents());
        CHECK_EQ(rfh1 != rfh2,
                 content::WillSameSiteNavigationChangeRenderFrameHosts(
                     /*is_main_frame=*/true));
        return rfh2;
      }

      case NavigationType::kCrossOrigin: {
        content::RenderFrameHost* rfh1 = main_frame();
        simulator->Commit();
        content::RenderFrameHost* rfh2 = simulator->GetFinalRenderFrameHost();
        CHECK_NE(rfh1, rfh2);
        return rfh2;
      }

      case NavigationType::kBackward:
        return simulator->GoBack(web_contents());

      case NavigationType::kPrerender: {
        content::RenderFrameHost* rfh2 =
            web_contents_tester->AddPrerenderAndCommitNavigation(kPrerenderUrl);
        CHECK_EQ(rfh2->GetLifecycleState(),
                 content::RenderFrameHost::LifecycleState::kPrerendering);
        return rfh2;
      }

      case NavigationType::kPrerenderedActivation:
        web_contents_tester->ActivatePrerenderedPage(kPrerenderUrl);
        CHECK_EQ(main_frame()->GetLastCommittedURL(), kPrerenderUrl);
        return main_frame();
    }
    NOTREACHED();
  }

 private:
  base::test::ScopedFeatureList bfcache_feature_list_{
      ::features::kBackForwardCache};
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::optional<content::test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
  FactoryObserver factory_observer_;
};

// Two helper macros to make tests below more readable.
#define EXPECT_DRIVER_CREATED(driver_ptr_ptr)                                  \
  EXPECT_CALL(observer(), OnContentAutofillDriverCreated(Ref(factory()),       \
                                                         HasState(kInactive))) \
      .WillOnce(DoAll(SaveArgPtr<1>((driver_ptr_ptr))))
#define EXPECT_LIFECYCLE_CHANGE(driver_matcher, from, to)                  \
  EXPECT_CALL(observer(),                                                  \
              OnContentAutofillDriverStateChanged(                         \
                  Ref(factory()), AllOf((driver_matcher), HasState((to))), \
                  (from), (to)))

// Tests the lifecycle state changes on a same-document navigation.
TEST_F(ContentAutofillDriverFactoryTestLifecycleState, NavigateSameDocument) {
  ContentAutofillDriver& driver1 = driver();
  EXPECT_CALL(observer(), OnContentAutofillDriverCreated).Times(0);
  EXPECT_CALL(observer(), OnContentAutofillDriverStateChanged).Times(0);
  Navigate(NavigationType::kSameDocument);
  ContentAutofillDriver& driver2 = driver();

  EXPECT_EQ(&driver1, &driver2);
  EXPECT_THAT(driver1, HasState(kActive));
}

// Tests the lifecycle state changes on a same-origin navigation.
// TODO(https://crbug.com/40615943): Remove this case when RenderDocument has
// fully launched, as it's almost identical to the cross-origin case.
TEST_F(ContentAutofillDriverFactoryTestLifecycleState, NavigateSameOrigin) {
  ContentAutofillDriver* driver1 = &driver();
  ContentAutofillDriver* driver2 = nullptr;
  if (content::WillSameSiteNavigationChangeRenderFrameHosts(
          /*is_main_frame=*/true)) {
    InSequence seq;
    EXPECT_DRIVER_CREATED(&driver2);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kInactive, kActive);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver1), kActive, kPendingDeletion);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kActive, kPendingReset);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kPendingReset, kActive);
  } else {
    InSequence seq;
    EXPECT_CALL(observer(), OnContentAutofillDriverCreated).Times(0);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver1), kActive, kPendingReset);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver1), kPendingReset, kActive);
  }

  Navigate(NavigationType::kSameOrigin);

  if (content::WillSameSiteNavigationChangeRenderFrameHosts(
          /*is_main_frame=*/true)) {
    EXPECT_NE(driver1, driver2);
    EXPECT_THAT(*driver2, HasState(kActive));
  } else {
    driver2 = &driver();
    EXPECT_EQ(driver1, driver2);
    EXPECT_THAT(*driver1, HasState(kActive));
  }
}

// Tests the lifecycle state changes on a cross-origin navigation.
TEST_F(ContentAutofillDriverFactoryTestLifecycleState, NavigateCrossOrigin) {
  ContentAutofillDriver& driver1 = driver();
  ContentAutofillDriver* driver2 = nullptr;
  {
    InSequence seq;
    EXPECT_DRIVER_CREATED(&driver2);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kInactive, kActive);
    EXPECT_LIFECYCLE_CHANGE(Ref(driver1), kActive, kInactive);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kActive, kPendingReset);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kPendingReset, kActive);
  }
  Navigate(NavigationType::kCrossOrigin);

  EXPECT_NE(&driver1, driver2);
  EXPECT_THAT(driver1, HasState(kInactive));
  EXPECT_THAT(*driver2, HasState(kActive));
}

// Tests the lifecycle state changes on a cross-origin navigation (or, more
// precisely, cross-RFH navigations).
TEST_F(ContentAutofillDriverFactoryTestLifecycleState, CloseTab) {
  EXPECT_THAT(driver(), HasState(kActive));
  {
    InSequence seq;
    EXPECT_LIFECYCLE_CHANGE(Ref(driver()), kActive, kPendingDeletion);
    EXPECT_CALL(observer(), OnContentAutofillDriverFactoryDestroyed)
        .WillOnce([&](ContentAutofillDriverFactory& factory) {
          factory.RemoveObserver(&observer());
        });
  }
  DeleteContents();
}

// Tests the lifecycle state changes when
// 1. a navigation happens
// 2. the reverse navigation is served from the BFCache.
TEST_F(ContentAutofillDriverFactoryTestLifecycleState, NavigateBFCached) {
  ContentAutofillDriver& driver1 = driver();
  ContentAutofillDriver* driver2 = nullptr;
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(checkpoint, Call("Navigation"));
    EXPECT_DRIVER_CREATED(&driver2);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kInactive, kActive);
    EXPECT_LIFECYCLE_CHANGE(Ref(driver1), kActive, kInactive);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kActive, kPendingReset);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kPendingReset, kActive);

    EXPECT_CALL(checkpoint, Call("Backward"));
    EXPECT_LIFECYCLE_CHANGE(Ref(driver1), kInactive, kActive);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kActive, kInactive);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Navigation");
  Navigate(NavigationType::kCrossOrigin);
  EXPECT_THAT(driver1, HasState(kInactive));
  EXPECT_THAT(*driver2, HasState(kActive));

  checkpoint.Call("Backward");
  Navigate(NavigationType::kBackward);
  checkpoint.Call("Finish");
  EXPECT_THAT(driver1, HasState(kActive));
  EXPECT_THAT(*driver2, HasState(kInactive));
}

// Tests the lifecycle state changes when
// 1. a frame is prerendered and
// 2. that frame is activated afterwards.
TEST_F(ContentAutofillDriverFactoryTestLifecycleState, NavigatePrerendering) {
  ContentAutofillDriver& driver1 = driver();
  ContentAutofillDriver* driver2 = nullptr;
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(checkpoint, Call("Prerender"));
    EXPECT_DRIVER_CREATED(&driver2);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kInactive, kPendingReset);
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kPendingReset, kInactive);

    EXPECT_CALL(checkpoint, Call("Activation"));
    EXPECT_LIFECYCLE_CHANGE(LazyRef(driver2), kInactive, kActive);
    EXPECT_LIFECYCLE_CHANGE(Ref(driver1), kActive, kInactive);
    EXPECT_CALL(checkpoint, Call("Finish"));
  }

  checkpoint.Call("Prerender");
  Navigate(NavigationType::kPrerender);
  EXPECT_THAT(driver1, HasState(kActive));
  EXPECT_THAT(*driver2, HasState(kInactive));

  checkpoint.Call("Activation");
  Navigate(NavigationType::kPrerenderedActivation);
  EXPECT_THAT(driver1, HasState(kInactive));
  EXPECT_THAT(*driver2, HasState(kActive));
  checkpoint.Call("Finish");
}

#undef EXPECT_LIFECYCLE_CHANGE
#undef EXPECT_DRIVER_CREATED

}  // namespace
}  // namespace autofill

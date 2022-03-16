// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::RenderFrameHost;
using testing::_;
using testing::Between;

namespace autofill {

namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));
};

}  // namespace

// Test case with one frame.
class ContentAutofillDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentAutofillDriverFactoryTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    factory_ = ContentAutofillDriverFactoryTestApi::Create(
        web_contents(), &client_, "en_US",
        BrowserAutofillManager::AutofillDownloadManagerState::
            ENABLE_AUTOFILL_DOWNLOAD_MANAGER,
        AutofillManager::AutofillManagerFactoryCallback());
    // One call of HideAutofillPopup() comes from ContentAutofillDriverFactory,
    // the second one from BrowserAutofillManager::Reset(). One of them may be
    // redundant.
    EXPECT_CALL(client_, HideAutofillPopup(_)).Times(Between(1, 2));
    NavigateMainFrame("https://a.com/");
  }

  void NavigateMainFrame(base::StringPiece url) {
    content::NavigationSimulator::CreateBrowserInitiated(GURL(url),
                                                         web_contents())
        ->Commit();
  }

  ContentAutofillDriverFactoryTestApi factory_test_api() {
    return ContentAutofillDriverFactoryTestApi(factory_.get());
  }

 protected:
  MockAutofillClient client_;
  std::unique_ptr<ContentAutofillDriverFactory> factory_;
};

TEST_F(ContentAutofillDriverFactoryTest, MainDriver) {
  ContentAutofillDriver* main_driver = factory_test_api().GetDriver(main_rfh());
  EXPECT_TRUE(main_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);
  EXPECT_EQ(factory_->DriverForFrame(main_rfh()), main_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);
  EXPECT_EQ(factory_->DriverForFrame(main_rfh()), main_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);
}

// Test case with two frames: the main frame and one child frame.
class ContentAutofillDriverFactoryTest_WithTwoFrames
    : public ContentAutofillDriverFactoryTest {
 public:
  void SetUp() override {
    ContentAutofillDriverFactoryTest::SetUp();
    CHECK(main_rfh());
    NavigateChildFrame("https://b.com/");
    CHECK(child_rfh_);
  }

  void NavigateChildFrame(base::StringPiece url) {
    if (!child_rfh_) {
      child_rfh_ = content::RenderFrameHostTester::For(main_rfh())
                       ->AppendChild(std::string("child"));
    }
    child_rfh_ = content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL(url), child_rfh_);
  }

  content::RenderFrameHost* child_rfh() { return child_rfh_; }

 private:
  raw_ptr<content::RenderFrameHost> child_rfh_ = nullptr;
};

TEST_F(ContentAutofillDriverFactoryTest_WithTwoFrames, TwoDrivers) {
  ASSERT_TRUE(main_rfh());
  ASSERT_TRUE(child_rfh());
  ContentAutofillDriver* main_driver = factory_->DriverForFrame(main_rfh());
  ContentAutofillDriver* child_driver = factory_->DriverForFrame(child_rfh());
  EXPECT_TRUE(main_driver);
  EXPECT_TRUE(child_driver);
  EXPECT_EQ(factory_->DriverForFrame(main_rfh()), main_driver);
  EXPECT_EQ(factory_->DriverForFrame(child_rfh()), child_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 2u);
  EXPECT_EQ(factory_->DriverForFrame(main_rfh()), main_driver);
  EXPECT_EQ(factory_->DriverForFrame(child_rfh()), child_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 2u);
  // TODO(crbug.com/1200511): Set the router's last source and target, and if
  // the |child_driver| is destroyed, expect a call to
  // AutofillManager::OnHidePopup(). For this to work, we need mock
  // AutofillManagers instead of real BrowserAutofillManager, which are blocked
  // by ContentAutofillDriver's use of the factory callback.
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

INSTANTIATE_TEST_SUITE_P(,
                         ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne,
                         testing::Bool());

// Tests that a driver is removed in RenderFrameDeleted().
TEST_P(ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne,
       RenderFrameDeleted) {
  ASSERT_TRUE(picked_rfh() == main_rfh() || picked_rfh() == child_rfh());
  ContentAutofillDriver* main_driver = factory_->DriverForFrame(main_rfh());
  ContentAutofillDriver* child_driver = factory_->DriverForFrame(child_rfh());
  EXPECT_TRUE(main_driver);
  EXPECT_TRUE(child_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 2u);
  factory_->RenderFrameDeleted(picked_rfh());
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);
  if (picked_rfh() == main_rfh())
    EXPECT_EQ(factory_test_api().GetDriver(child_rfh()), child_driver);
  else
    EXPECT_EQ(factory_test_api().GetDriver(main_rfh()), main_driver);
}

// Tests that OnVisibilityChanged() hides the popup.
TEST_F(ContentAutofillDriverFactoryTest, TabHidden) {
  EXPECT_CALL(client_, HideAutofillPopup(PopupHidingReason::kTabGone));
  factory_->OnVisibilityChanged(content::Visibility::HIDDEN);
}

// Test case with one frame, with BFcache and AutofillAcrossIframes enabled or
// disabled depending on the parameter.
class ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes
    : public ContentAutofillDriverFactoryTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes() {
    std::vector<base::Feature> enabled;
    // Allow BackForwardCache for all devices regardless of their memory.
    std::vector<base::Feature> disabled{
        ::features::kBackForwardCacheMemoryControls};
    (autofill_across_iframes() ? enabled : disabled)
        .push_back(features::kAutofillAcrossIframes);
    (use_bfcache() ? enabled : disabled)
        .push_back(::features::kBackForwardCache);
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  bool use_bfcache() { return std::get<0>(GetParam()); }
  bool autofill_across_iframes() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
    testing::Combine(testing::Bool(), testing::Bool()));

// Tests that that a same-documentation navigation does not touch the factory's
// router.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       SameDocumentNavigation) {
  content::RenderFrameHost* orig_rfh = main_rfh();
  ContentAutofillDriver* orig_driver = factory_->DriverForFrame(orig_rfh);

  // One call of HideAutofillPopup() comes from ContentAutofillDriverFactory,
  // the second one from BrowserAutofillManager::Reset(). One of them may be
  // redundant.
  EXPECT_CALL(client_, HideAutofillPopup(PopupHidingReason::kNavigation))
      .Times(Between(1, 2));

  NavigateMainFrame("https://a.com/#same-site");

  ASSERT_EQ(orig_rfh, main_rfh());
  EXPECT_EQ(factory_test_api().GetDriver(orig_rfh), orig_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);

  // TODO(crbug.com/1200511): Test that |router_| has been untouched. To this
  // end, call `orig_driver->FormsSeen({FormData{}})` above and then check
  // here that the router still knows that form. For this to work, we need mock
  // AutofillManagers instead of real BrowserAutofillManager, which are blocked
  // by ContentAutofillDriver's use of the factory callback.
}

// Tests that that a driver survives a same-origin navigation but is reset
// afterwards.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       SameOriginNavigation) {
  content::RenderFrameHost* orig_rfh = main_rfh();
  ContentAutofillDriver* orig_driver = factory_->DriverForFrame(orig_rfh);

  // TODO(crbug.com/1200511): Use mock AutofillManagers and expect a call of
  // AutofillManager::Reset(), which is blocked by ContentAutofillDriver's use
  // of the factory callback.

  // One call of HideAutofillPopup() comes from ContentAutofillDriverFactory,
  // the second one from BrowserAutofillManager::Reset(). One of them may be
  // redundant.
  EXPECT_CALL(client_, HideAutofillPopup(PopupHidingReason::kNavigation))
      .Times(Between(1, 2));

  NavigateMainFrame("https://a.com/after-navigation");

  EXPECT_EQ(factory_test_api().GetDriver(orig_rfh), orig_driver);
  // If BFCache is enabled, there will be 2 drivers as the old document is still
  // around.
  EXPECT_EQ(factory_test_api().num_drivers(), use_bfcache() ? 2u : 1u);
}

// Tests that that a driver is removed and replaced with a fresh one after a
// cross-origin navigation.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       CrossOriginNavigation) {
  content::RenderFrameHost* orig_rfh = main_rfh();
  content::GlobalRenderFrameHostId orig_rfh_id = orig_rfh->GetGlobalId();
  ContentAutofillDriver* orig_driver = factory_->DriverForFrame(orig_rfh);

  ASSERT_EQ(orig_rfh, main_rfh());
  EXPECT_EQ(factory_test_api().GetDriver(orig_rfh), orig_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);

  // One call of HideAutofillPopup() comes from ContentAutofillDriverFactory,
  // the second one from BrowserAutofillManager::Reset(). One of them may be
  // redundant.
  EXPECT_CALL(client_, HideAutofillPopup(PopupHidingReason::kNavigation))
      .Times(Between(1, 2));

  NavigateMainFrame("https://different-origin-after-navigation.com/");

  ASSERT_NE(orig_rfh_id, main_rfh()->GetGlobalId());
  // A new driver for main_rfh() has been created and the |orig_rfh| has now
  // been removed in ContentAutofillDriverFactory::RenderFrameDeleted(), unless
  // BFcache is enabled (or main_rfh() happens to have the same address as
  // |orig_rfh|).
  if (use_bfcache())
    EXPECT_EQ(factory_test_api().GetDriver(orig_rfh), orig_driver);
  else if (main_rfh() != orig_rfh)
    EXPECT_EQ(factory_test_api().GetDriver(orig_rfh), nullptr);
  EXPECT_NE(factory_test_api().GetDriver(main_rfh()), nullptr);
  EXPECT_EQ(factory_test_api().num_drivers(), use_bfcache() ? 2u : 1u);
}

}  // namespace autofill

// Copyright 2021 The Chromium Authors
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
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

using testing::_;
using testing::AtLeast;
using testing::Between;

namespace autofill {

namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));
};

class MockAutofillAgent : public mojom::AutofillAgent {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillAgent>(
                             std::move(handle)));
  }

  MOCK_METHOD(void, TriggerReparse, (), (override));
  MOCK_METHOD(void,
              FillOrPreviewForm,
              (const FormData& form, mojom::RendererFormDataAction action),
              (override));
  MOCK_METHOD(void,
              FieldTypePredictionsAvailable,
              (const std::vector<FormDataPredictions>& forms),
              (override));
  MOCK_METHOD(void, ClearSection, (), (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              FillFieldWithValue,
              (FieldRendererId field, const std::u16string& value),
              (override));
  MOCK_METHOD(void,
              PreviewFieldWithValue,
              (FieldRendererId field, const ::std::u16string& value),
              (override));
  MOCK_METHOD(void,
              SetSuggestionAvailability,
              (FieldRendererId field, mojom::AutofillState type),
              (override));
  MOCK_METHOD(void,
              AcceptDataListSuggestion,
              (FieldRendererId field, const ::std::u16string& value),
              (override));
  MOCK_METHOD(void,
              FillPasswordSuggestion,
              (const ::std::u16string& username,
               const ::std::u16string& password),
              (override));
  MOCK_METHOD(void,
              PreviewPasswordSuggestion,
              (const ::std::u16string& username,
               const ::std::u16string& password),
              (override));
  MOCK_METHOD(void,
              PreviewPasswordGenerationSuggestion,
              (const ::std::u16string& password),
              (override));
  MOCK_METHOD(void, SetUserGestureRequired, (bool required), (override));
  MOCK_METHOD(void, SetSecureContextRequired, (bool required), (override));
  MOCK_METHOD(void, SetFocusRequiresScroll, (bool require), (override));
  MOCK_METHOD(void, SetQueryPasswordSuggestion, (bool query), (override));
  MOCK_METHOD(void,
              GetElementFormAndFieldDataForDevToolsNodeId,
              (int32_t backend_node_id,
               GetElementFormAndFieldDataForDevToolsNodeIdCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAssistantKeyboardSuppressState,
              (bool suppress),
              (override));
  MOCK_METHOD(void, EnableHeavyFormDataScraping, (), (override));
  MOCK_METHOD(void,
              SetFieldsEligibleForManualFilling,
              (const std::vector<FieldRendererId>& fields),
              (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillAgent> receivers_;
};

}  // namespace

// Test case with one frame.
class ContentAutofillDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  explicit ContentAutofillDriverFactoryTest(
      version_info::Channel channel = version_info::Channel::UNKNOWN)
      : channel_(channel) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    client_ = std::make_unique<MockAutofillClient>();
    client_->set_channel_for_testing(channel_);

    agent_ = std::make_unique<MockAutofillAgent>();
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillAgent::Name_,
        base::BindRepeating(&MockAutofillAgent::BindPendingReceiver,
                            base::Unretained(agent_.get())));

    factory_ = ContentAutofillDriverFactoryTestApi::Create(
        web_contents(), client_.get(),
        base::BindRepeating(&autofill::BrowserDriverInitHook, client_.get(),
                            "en-US"));
  }

  void TearDown() override {
    factory_.reset();
    client_.reset();
    agent_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void NavigateMainFrame(base::StringPiece url) {
    // One call of HideAutofillPopup() comes from ContentAutofillDriverFactory.
    // A second one may come from BrowserAutofillManager::Reset().
    EXPECT_CALL(*client_, HideAutofillPopup(PopupHidingReason::kNavigation))
        .Times(Between(1, 2));
    content::NavigationSimulator::CreateBrowserInitiated(GURL(url),
                                                         web_contents())
        ->Commit();
  }

  ContentAutofillDriverFactoryTestApi factory_test_api() {
    return ContentAutofillDriverFactoryTestApi(factory_.get());
  }

 protected:
  version_info::Channel channel_;
  std::unique_ptr<MockAutofillAgent> agent_;
  std::unique_ptr<MockAutofillClient> client_;
  std::unique_ptr<ContentAutofillDriverFactory> factory_;
};

TEST_F(ContentAutofillDriverFactoryTest, MainDriver) {
  NavigateMainFrame("https://a.com/");
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
  void NavigateChildFrame(base::StringPiece url) {
    CHECK(main_rfh());
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
  NavigateMainFrame("https://a.com/");
  NavigateChildFrame("https://b.com/");
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

INSTANTIATE_TEST_SUITE_P(ContentAutofillDriverFactoryTest,
                         ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne,
                         testing::Bool());

// Tests that a driver is removed in RenderFrameDeleted().
TEST_P(ContentAutofillDriverFactoryTest_WithTwoFrames_PickOne,
       RenderFrameDeleted) {
  NavigateMainFrame("https://a.com/");
  NavigateChildFrame("https://b.com/");
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
  NavigateMainFrame("https://a.com/");
  EXPECT_CALL(*client_, HideAutofillPopup(PopupHidingReason::kTabGone));
  factory_->OnVisibilityChanged(content::Visibility::HIDDEN);
}

// Test case with one frame, with BFcache and AutofillAcrossIframes enabled or
// disabled depending on the parameter.
class ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes
    : public ContentAutofillDriverFactoryTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes() {
    std::vector<base::test::FeatureRef> enabled;
    // Allow BackForwardCache for all devices regardless of their memory.
    std::vector<base::test::FeatureRef> disabled{
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
    ContentAutofillDriverFactoryTest,
    ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
    testing::Combine(testing::Bool(), testing::Bool()));

// Tests that that a same-documentation navigation does not touch the factory's
// router.
TEST_P(ContentAutofillDriverFactoryTest_WithOrWithoutBfCacheAndIframes,
       SameDocumentNavigation) {
  NavigateMainFrame("https://a.com/");
  content::RenderFrameHost* orig_rfh = main_rfh();
  ContentAutofillDriver* orig_driver = factory_->DriverForFrame(orig_rfh);
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
  NavigateMainFrame("https://a.com/");
  content::RenderFrameHost* orig_rfh = main_rfh();
  ContentAutofillDriver* orig_driver = factory_->DriverForFrame(orig_rfh);

  // TODO(crbug.com/1200511): Use mock AutofillManagers and expect a call of
  // AutofillManager::Reset(), which is blocked by ContentAutofillDriver's use
  // of the factory callback.

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
  NavigateMainFrame("https://a.com/");
  content::RenderFrameHost* orig_rfh = main_rfh();
  content::GlobalRenderFrameHostId orig_rfh_id = orig_rfh->GetGlobalId();
  ContentAutofillDriver* orig_driver = factory_->DriverForFrame(orig_rfh);

  ASSERT_EQ(orig_rfh, main_rfh());
  EXPECT_EQ(factory_test_api().GetDriver(orig_rfh), orig_driver);
  EXPECT_EQ(factory_test_api().num_drivers(), 1u);

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

// Fixture for testing that Autofill is enabled in fenced frames unless
// AutofillEnableWithinFencedFrame is enabled. The bool parameter
// enables/disables that feature.
class ContentAutofillDriverFactoryTest_FencedFrames
    : public ContentAutofillDriverFactoryTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ContentAutofillDriverFactoryTest_FencedFrames() {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    enabled.push_back(
        {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}});
    if (autofill_enabled_in_fencedframe()) {
      enabled.push_back({features::kAutofillEnableWithinFencedFrame, {}});
    } else {
      disabled.push_back(features::kAutofillEnableWithinFencedFrame);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  ~ContentAutofillDriverFactoryTest_FencedFrames() override = default;

  bool autofill_enabled_in_fencedframe() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ContentAutofillDriverFactoryTest_FencedFrames,
       DisableAutofillWithinFencedFrame) {
  NavigateMainFrame("http://test.org");
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  content::RenderFrameHost* fenced_frame_subframe =
      content::RenderFrameHostTester::For(fenced_frame_root)
          ->AppendChild("iframe");
  EXPECT_NE(nullptr, factory_->DriverForFrame(main_rfh()));
  if (autofill_enabled_in_fencedframe()) {
    EXPECT_NE(nullptr, factory_->DriverForFrame(fenced_frame_root));
    EXPECT_NE(nullptr, factory_->DriverForFrame(fenced_frame_subframe));
  } else {
    EXPECT_EQ(nullptr, factory_->DriverForFrame(fenced_frame_root));
    EXPECT_EQ(nullptr, factory_->DriverForFrame(fenced_frame_subframe));
  }
}

INSTANTIATE_TEST_SUITE_P(ContentAutofillDriverFactoryTest,
                         ContentAutofillDriverFactoryTest_FencedFrames,
                         testing::Bool());

struct AgentSetupParam {
  version_info::Channel channel;
  bool heavy_scraping_enabled;
};

class ContentAutofillDriverFactoryTest_AgentSetup
    : public ContentAutofillDriverFactoryTest,
      public ::testing::WithParamInterface<AgentSetupParam> {
 public:
  ContentAutofillDriverFactoryTest_AgentSetup()
      : ContentAutofillDriverFactoryTest(GetParam().channel) {}
};

INSTANTIATE_TEST_SUITE_P(
    ContentAutofillDriverFactoryTest,
    ContentAutofillDriverFactoryTest_AgentSetup,
    testing::Values(AgentSetupParam{version_info::Channel::CANARY, true},
                    AgentSetupParam{version_info::Channel::DEV, true},
                    AgentSetupParam{version_info::Channel::UNKNOWN, false},
                    AgentSetupParam{version_info::Channel::BETA, false},
                    AgentSetupParam{version_info::Channel::STABLE, false}));

TEST_P(ContentAutofillDriverFactoryTest_AgentSetup,
       EnableHeavyFormDataScraping) {
  EXPECT_CALL(*agent_, EnableHeavyFormDataScraping())
      .Times(GetParam().heavy_scraping_enabled ? 1 : 0);
  NavigateMainFrame("https://a.com/");
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(agent_.get());
}

}  // namespace autofill

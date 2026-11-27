// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/web_payments_observer.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/payments/core/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

}  // namespace

class WebPaymentsObserverTest : public content::RenderViewHostTestHarness {
 public:
  ~WebPaymentsObserverTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    observer_ = std::make_unique<WebPaymentsObserver>(web_contents());
  }

  void TearDown() override {
    observer_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Returns the UKM source ID of the next page of the navigation.
  ukm::SourceId TriggerDidStartNavigation(
      bool is_post,
      bool is_form_submission,
      scoped_refptr<network::ResourceRequestBody> post_data,
      bool provide_navigation_entry = true) {
    testing::NiceMock<content::MockNavigationHandle> handle(web_contents());
    ON_CALL(handle, IsPost()).WillByDefault(testing::Return(is_post));
    handle.set_is_form_submission(is_form_submission);
    handle.set_post_data(post_data);

    observer_->DidStartNavigation(&handle);
    return handle.GetNextPageUkmSourceId();
  }

  // Returns the UKM source ID of the next page of the navigation.
  ukm::SourceId TriggerDidStartNavigationWithData(
      const std::string& body_string,
      bool is_post = true,
      bool is_form_submission = true) {
    scoped_refptr<network::ResourceRequestBody> post_data =
        network::ResourceRequestBody::CreateFromCopyOfBytes(
            base::as_byte_span(body_string));
    return TriggerDidStartNavigation(is_post, is_form_submission, post_data);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kThreeDSecureTelemetry};
  std::unique_ptr<WebPaymentsObserver> observer_;
};

TEST_F(WebPaymentsObserverTest, FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(features::kThreeDSecureTelemetry);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=test&cres=test");
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, NullNavigationHandle) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  observer_->DidStartNavigation(nullptr);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, NotPostRequest) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=test&cres=test",
                                    /*is_post=*/false,
                                    /*is_form_submission=*/true);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, NotFormSubmission) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=test&cres=test",
                                    /*is_post=*/true,
                                    /*is_form_submission=*/false);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, NullPostData) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigation(/*is_post=*/true, /*is_form_submission=*/true,
                            /*post_data=*/nullptr,
                            /*provide_navigation_entry=*/true);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, EmptyPostDataElements) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto post_data = base::MakeRefCounted<network::ResourceRequestBody>();
  TriggerDidStartNavigation(/*is_post=*/true, /*is_form_submission=*/true,
                            post_data);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, NonBytesPostDataElement) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto post_data = base::MakeRefCounted<network::ResourceRequestBody>();
  post_data->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("/dummy/path")),
                             /*offset=*/0, /*length=*/100,
                             /*expected_modification_time=*/base::Time());
  TriggerDidStartNavigation(/*is_post=*/true, /*is_form_submission=*/true,
                            post_data);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

TEST_F(WebPaymentsObserverTest, RecordsTelemetryFor3DSFormSubmission) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId next_page_source_id =
      TriggerDidStartNavigationWithData("creq=sample_challenge_request");
  histogram_tester.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id, next_page_source_id);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
          kChallengeRequestName,
      true);
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

class WebPaymentsObserverCheckoutTelemetryTest
    : public WebPaymentsObserverTest {
 public:
  WebPaymentsObserverCheckoutTelemetryTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(features::kCheckoutTelemetry);
  }

  void NavigateAndSetTitle(std::string_view url, std::u16string_view title) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               GURL(url));
    SetCommittedTitle(title);
  }

  void SetCommittedTitle(std::u16string_view title) {
    web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                        std::u16string(title));
  }
};

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       ClassifiesCheckoutAndOrderConfirmationPerNavigation) {
  NavigateAndSetTitle("https://store.example/checkout", u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());

  NavigateAndSetTitle("https://store.example/order-confirmation",
                      u"Order Confirmation");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  EXPECT_TRUE(observer_->classified_order_confirmation_for_testing());
  NavigateAndSetTitle("chrome://settings/payments", u"Checkout");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());
  NavigateAndSetTitle("https://store.example/", u"Store Home");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       ClassifiesBothStagesOnInPlaceTitleTransition) {
  NavigateAndSetTitle("https://store.example/", u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());

  SetCommittedTitle(u"Order Confirmation");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
  EXPECT_TRUE(observer_->classified_order_confirmation_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       ClassifiesAgainAfterSameDocumentNavigation) {
  NavigateAndSetTitle("https://store.example/checkout", u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://store.example/order-confirmation"), main_rfh())
      ->CommitSameDocument();
  // Nothing is classified until the title is set.
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());

  SetCommittedTitle(u"Order Confirmation");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  EXPECT_TRUE(observer_->classified_order_confirmation_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest, UsesEntryUrlAndRawTitle) {
  // Set a title before clearing it because TitleWasSet() only fires when the
  // title changes. With an empty title, the display title falls back to the URL
  // (which would match the title regex), and both the virtual URL and pending
  // URL point at /checkout (which would match the URL regex).
  NavigateAndSetTitle("https://store.example/pre-checkout", u"Store Home");
  controller().GetLastCommittedEntry()->SetVirtualURL(
      GURL("https://store.example/checkout"));
  auto pending = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://store.example/checkout"), web_contents());
  pending->Start();
  SetCommittedTitle(u"");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());

  NavigateAndSetTitle("https://store.example/checkout", u"Store Home");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       IgnoresTitleOfNonCommittedEntries) {
  NavigateAndSetTitle("https://store.example/", u"Store Home");
  content::NavigationEntry* earlier_entry =
      controller().GetLastCommittedEntry();
  NavigateAndSetTitle("https://store.example/cart", u"Cart");
  ASSERT_NE(earlier_entry, controller().GetLastCommittedEntry());

  web_contents()->UpdateTitleForEntry(earlier_entry, u"Order Confirmation");
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());

  auto pending = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://store.example/checkout"), web_contents());
  pending->Start();
  ASSERT_NE(controller().GetPendingEntry(), nullptr);
  web_contents()->UpdateTitleForEntry(controller().GetPendingEntry(),
                                      u"Checkout");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       IgnoresUncommittedAndSubframeNavigations) {
  NavigateAndSetTitle("https://store.example/checkout", u"Checkout");
  auto aborted = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://store.example/cart"), web_contents());
  aborted->Start();
  aborted->AbortCommit();
  EXPECT_TRUE(observer_->classified_checkout_for_testing());

  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://store.example/frame"), child);
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       DoesNotClassifyWhenFeatureDisabled) {
  NavigateAndSetTitle("https://store.example/cart", u"Cart");
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(features::kCheckoutTelemetry);
  SetCommittedTitle(u"Checkout");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  NavigateAndSetTitle("https://store.example/checkout", u"Checkout");
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  NavigateAndSetTitle("https://store.example/order-confirmation",
                      u"Order Confirmation");
  EXPECT_FALSE(observer_->classified_order_confirmation_for_testing());
}

TEST_F(WebPaymentsObserverCheckoutTelemetryTest,
       HandlesRepeatedIdenticalUrlAndTitle) {
  const GURL url("https://store.example/checkout");
  NavigateAndSetTitle(url.spec(), u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
  SetCommittedTitle(u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
  content::NavigationSimulator::CreateRendererInitiated(url, main_rfh())
      ->CommitSameDocument();
  SetCommittedTitle(u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
  auto post =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  post->SetMethod("POST");
  post->Commit();
  EXPECT_FALSE(observer_->classified_checkout_for_testing());
  SetCommittedTitle(u"Checkout");
  EXPECT_TRUE(observer_->classified_checkout_for_testing());
}

}  // namespace payments

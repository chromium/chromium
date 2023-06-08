// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using security_interstitials::UnsafeResource;

class MockUrlCheckerDelegate : public UrlCheckerDelegate {
 public:
  MockUrlCheckerDelegate() = default;

  MOCK_METHOD1(MaybeDestroyNoStatePrefetchContents,
               void(base::OnceCallback<content::WebContents*()>));
  MOCK_METHOD5(StartDisplayingBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&,
                    const std::string&,
                    const net::HttpRequestHeaders&,
                    bool,
                    bool));
  MOCK_METHOD2(StartObservingInteractionsForDelayedBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&, bool));
  MOCK_METHOD1(NotifySuspiciousSiteDetected,
               void(const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD0(GetUIManager, BaseUIManager*());
  MOCK_METHOD0(GetThreatTypes, const SBThreatTypeSet&());
  MOCK_METHOD1(IsUrlAllowlisted, bool(const GURL&));
  MOCK_METHOD1(SetPolicyAllowlistDomains,
               void(const std::vector<std::string>&));
  MOCK_METHOD3(CheckLookupMechanismExperimentEligibility,
               void(const security_interstitials::UnsafeResource&,
                    base::OnceCallback<void(bool)>,
                    scoped_refptr<base::SequencedTaskRunner>));
  MOCK_METHOD3(CheckExperimentEligibilityAndStartBlockingPage,
               void(const security_interstitials::UnsafeResource&,
                    base::OnceCallback<void(bool)>,
                    scoped_refptr<base::SequencedTaskRunner>));

  SafeBrowsingDatabaseManager* GetDatabaseManager() override { return nullptr; }

  bool ShouldSkipRequestCheck(const GURL& original_url,
                              int frame_tree_node_id,
                              int render_process_id,
                              int render_frame_id,
                              bool originated_from_service_worker) override {
    return should_skip_request_check_;
  }
  void EnableSkipRequestCheck() { should_skip_request_check_ = true; }

 protected:
  ~MockUrlCheckerDelegate() override = default;

 private:
  bool should_skip_request_check_ = false;
};

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  ~MockThrottleDelegate() override = default;

  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {
    error_code_ = error_code;
    custom_reason_ = custom_reason;
  }
  void Resume() override { is_resumed_ = true; }

  int GetErrorCode() { return error_code_; }
  base::StringPiece GetCustomReason() { return custom_reason_; }
  bool IsResumed() { return is_resumed_; }

 private:
  int error_code_ = 0;
  base::StringPiece custom_reason_ = "";
  bool is_resumed_ = false;
};

class MockSafeBrowsingUrlChecker : public SafeBrowsingUrlCheckerImpl {
 public:
  struct CallbackInfo {
    bool should_proceed = true;
    bool should_show_interstitial = false;
    bool should_delay_callback = false;
    NativeCheckUrlCallback callback;
  };

  MockSafeBrowsingUrlChecker(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      UnsafeResource::RenderProcessId render_process_id,
      UnsafeResource::RenderFrameId render_frame_id,
      UnsafeResource::FrameTreeNodeId frame_tree_node_id,
      bool url_real_time_lookup_enabled,
      bool can_urt_check_subresource_url,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      GURL last_committed_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      UrlRealTimeMechanism::WebUIDelegate* webui_delegate,
      base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui,
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter,
      bool is_mechanism_experiment_allowed,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection)
      : SafeBrowsingUrlCheckerImpl(headers,
                                   load_flags,
                                   request_destination,
                                   has_user_gesture,
                                   url_checker_delegate,
                                   web_contents_getter,
                                   render_process_id,
                                   render_frame_id,
                                   frame_tree_node_id,
                                   url_real_time_lookup_enabled,
                                   can_urt_check_subresource_url,
                                   can_check_db,
                                   can_check_high_confidence_allowlist,
                                   url_lookup_service_metric_suffix,
                                   last_committed_url,
                                   ui_task_runner,
                                   url_lookup_service_on_ui,
                                   webui_delegate,
                                   hash_realtime_service_on_ui,
                                   mechanism_experimenter,
                                   is_mechanism_experiment_allowed,
                                   hash_realtime_selection) {}

  // Returns the CallbackInfo that was previously added in |AddCallbackInfo|.
  // It will crash if |AddCallbackInfo| was not called.
  // If |should_delay_callback| was set to true, the callback will be cached.
  // The callback can be invoked manually via |RestartDelayedCallback|.
  // Otherwise, the callback will be invoked immediately.
  void CheckUrl(const GURL& url,
                const std::string& method,
                NativeCheckUrlCallback callback) override {
    ASSERT_GT(callback_infos_.size(), check_url_called_cnt_);
    CallbackInfo& callback_info = callback_infos_[check_url_called_cnt_++];
    if (callback_info.should_delay_callback) {
      callback_info.callback = std::move(callback);
    } else {
      std::move(callback).Run(/*slow_check_notifier=*/nullptr,
                              /*proceed=*/callback_info.should_proceed,
                              /*show_interstitial=*/
                              callback_info.should_show_interstitial,
                              /*did_perform_url_real_time_check=*/false,
                              /*did_check_url_real_time_allowlist=*/false);
    }
  }

  void RestartDelayedCallback(size_t index) {
    ASSERT_GT(callback_infos_.size(), index);
    ASSERT_TRUE(callback_infos_[index].should_delay_callback);
    std::move(callback_infos_[index].callback)
        .Run(/*slow_check_notifier=*/nullptr,
             /*proceed=*/callback_infos_[index].should_proceed,
             /*show_interstitial=*/
             callback_infos_[index].should_show_interstitial,
             /*did_perform_url_real_time_check=*/false,
             /*did_check_url_real_time_allowlist=*/false);
  }

  // Informs how the callback in |CheckUrl| should be handled. The info applies
  // to the callback sent in |CheckUrl| in sequence. That is, the first info
  // added will be applied to the first call of |CheckUrl|.
  void AddCallbackInfo(bool should_proceed,
                       bool should_show_interstitial,
                       bool should_delay_callback) {
    CallbackInfo callback_info;
    callback_info.should_proceed = should_proceed;
    callback_info.should_show_interstitial = should_show_interstitial;
    callback_info.should_delay_callback = should_delay_callback;
    callback_infos_.push_back(std::move(callback_info));
  }

 private:
  size_t check_url_called_cnt_ = 0;
  std::vector<CallbackInfo> callback_infos_;
};

}  // namespace

class SBBrowserUrlLoaderThrottleTest : public ::testing::Test {
 protected:
  SBBrowserUrlLoaderThrottleTest() = default;

  scoped_refptr<UrlCheckerDelegate> GetUrlCheckerDelegate() {
    return url_checker_delegate_;
  }

  void SetUp() override {
    auto url_checker_delegate_getter = base::BindOnce(
        [](SBBrowserUrlLoaderThrottleTest* test) {
          return test->GetUrlCheckerDelegate();
        },
        base::Unretained(this));
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    EXPECT_CALL(mock_web_contents_getter, Run())
        .WillOnce(testing::Return(nullptr));
    throttle_ = BrowserURLLoaderThrottle::Create(
        std::move(url_checker_delegate_getter), mock_web_contents_getter.Get(),
        /*frame_tree_node_id=*/0, /*url_lookup_service=*/nullptr,
        /*hash_realtime_service=*/nullptr, /*ping_manager=*/nullptr,
        /*hash_realtime_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);

    url_checker_delegate_ = base::MakeRefCounted<MockUrlCheckerDelegate>();
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();

    std::unique_ptr<MockSafeBrowsingUrlChecker> url_checker =
        std::make_unique<MockSafeBrowsingUrlChecker>(
            net::HttpRequestHeaders(), /*load_flags=*/0,
            network::mojom::RequestDestination::kDocument,
            /*has_user_gesture=*/false, url_checker_delegate_,
            mock_web_contents_getter.Get(), UnsafeResource::kNoRenderProcessId,
            UnsafeResource::kNoRenderFrameId,
            UnsafeResource::kNoFrameTreeNodeId,
            /*url_real_time_lookup_enabled=*/false,
            /*can_urt_check_subresource_url=*/false, /*can_check_db=*/true,
            /*can_check_high_confidence_allowlist=*/true,
            /*url_lookup_service_metric_suffix=*/"",
            /*last_committed_url=*/GURL(),
            /*ui_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
            /*url_lookup_service_on_ui=*/nullptr,
            /*webui_delegate_=*/nullptr,
            /*hash_realtime_service_on_ui=*/nullptr,
            /*mechanism_experimenter=*/nullptr,
            /*is_mechanism_experiment_allowed=*/false,
            /*hash_realtime_selection=*/
            hash_realtime_utils::HashRealTimeSelection::kNone);
    url_checker_ = url_checker.get();

    throttle_->GetSBCheckerForTesting()->SetUrlCheckerForTesting(
        std::move(url_checker));
    throttle_->set_delegate(throttle_delegate_.get());

    url_ = GURL("https://example.com/");
    response_head_ = network::mojom::URLResponseHead::New();
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillStartRequest() {
    bool defer = false;
    network::ResourceRequest request;
    request.url = url_;
    throttle_->WillStartRequest(&request, &defer);
    task_environment_.RunUntilIdle();
    return defer;
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillRedirectRequest() {
    bool defer = false;
    net::RedirectInfo redirect_info;
    std::vector<std::string> to_be_removed_headers;
    net::HttpRequestHeaders modified_headers;
    net::HttpRequestHeaders modified_cors_exempt_headers;
    throttle_->WillRedirectRequest(&redirect_info, *response_head_, &defer,
                                   &to_be_removed_headers, &modified_headers,
                                   &modified_cors_exempt_headers);
    task_environment_.RunUntilIdle();
    return defer;
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillProcessResponse() {
    bool defer = false;
    throttle_->WillProcessResponse(url_, response_head_.get(), &defer);
    task_environment_.RunUntilIdle();
    return defer;
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillProcessResponseFromCache() {
    bool defer = false;
    response_head_->was_fetched_via_cache = true;
    response_head_->network_accessed = false;
    throttle_->WillProcessResponse(url_, response_head_.get(), &defer);
    task_environment_.RunUntilIdle();
    return defer;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  GURL url_;
  network::mojom::URLResponseHeadPtr response_head_;
  std::unique_ptr<BrowserURLLoaderThrottle> throttle_;
  // Owned by |throttle_|. May be deleted before the test completes. Prefer
  // setting it up at the start of the test.
  raw_ptr<MockSafeBrowsingUrlChecker, DanglingUntriaged> url_checker_;
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
};

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnSafeUrl) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), 0);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnUnSafeUrl) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  // Safe Browsing and URL loader are performed in parallel. Safe Browsing
  // doesn't defer the start of the request.
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_EQ(throttle_delegate_->GetCustomReason(), "SafeBrowsing");

  defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferIfRedirectUrlIsUnsafe) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  CallWillRedirectRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnSkippedUrl) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);
  url_checker_delegate_->EnableSkipRequestCheck();

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  // The loader is not deferred because the check has skipped.
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnKnownSafeUrl) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  bool defer = false;
  network::ResourceRequest request;
  request.url = GURL("chrome://new-tab-page");
  throttle_->WillStartRequest(&request, &defer);
  task_environment_.RunUntilIdle();

  CallWillRedirectRequest();

  defer = CallWillProcessResponse();
  // The loader is not deferred because the URL is known to be safe.
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnSlowCheck) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  // Deferred because the check has not completed.
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnSlowRedirectCheck) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillRedirectRequest();

  bool defer = CallWillProcessResponse();
  // Deferred because the check for redirect URL has not completed.
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  url_checker_->RestartDelayedCallback(/*index=*/1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotResumeOnSlowCheckNotProceed) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  // Resume should not be called because the slow check returns don't proceed.
  EXPECT_FALSE(throttle_delegate_->IsResumed());
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferRedirectOnSlowCheck) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();

  bool defer = CallWillRedirectRequest();
  // Although the first check has not completed, redirect should not be
  // deferred.
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DeferRedirectWhenFirstUrlAlreadyBlocked) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillRedirectRequest();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyErrorCodeWhenInterstitialNotShown) {
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_ABORTED);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_FastCheckFromNetwork) {
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  CallWillProcessResponse();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashBasedCheck",
      base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_FastCheckFromCache) {
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  CallWillProcessResponseFromCache();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashBasedCheck",
      base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SlowCheckFromNetwork) {
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillProcessResponse();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashBasedCheck",
      base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SlowCheckFromCache) {
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillProcessResponseFromCache();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashBasedCheck",
      base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork", 0);
}

}  // namespace safe_browsing

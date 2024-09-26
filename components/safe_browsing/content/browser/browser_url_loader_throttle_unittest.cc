// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/url_checker_holder.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
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
  MOCK_METHOD4(StartDisplayingBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&,
                    const std::string&,
                    const net::HttpRequestHeaders&,
                    bool));
  MOCK_METHOD1(StartObservingInteractionsForDelayedBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&));
  MOCK_METHOD1(NotifySuspiciousSiteDetected,
               void(const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD0(GetUIManager, BaseUIManager*());
  MOCK_METHOD0(GetThreatTypes, const SBThreatTypeSet&());
  MOCK_METHOD1(IsUrlAllowlisted, bool(const GURL&));
  MOCK_METHOD1(SetPolicyAllowlistDomains,
               void(const std::vector<std::string>&));
  MOCK_METHOD2(SendUrlRealTimeAndHashRealTimeDiscrepancyReport,
               void(std::unique_ptr<ClientSafeBrowsingReportRequest>,
                    const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD1(AreBackgroundHashRealTimeSampleLookupsAllowed,
               bool(const base::RepeatingCallback<content::WebContents*()>&));

  SafeBrowsingDatabaseManager* GetDatabaseManager() override { return nullptr; }

  bool ShouldSkipRequestCheck(
      const GURL& original_url,
      int frame_tree_node_id,
      int render_process_id,
      base::optional_ref<const base::UnguessableToken> render_frame_token,
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
                       std::string_view custom_reason) override {
    error_code_ = error_code;
    custom_reason_ = custom_reason;
  }
  void Resume() override { is_resumed_ = true; }

  int GetErrorCode() { return error_code_; }
  std::string_view GetCustomReason() { return custom_reason_; }
  bool IsResumed() { return is_resumed_; }

 private:
  int error_code_ = 0;
  std::string_view custom_reason_ = "";
  bool is_resumed_ = false;
};

class FakeRealTimeUrlLookupService
    : public testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // RealTimeUrlLookupServiceBase:
  void StartLookup(
      const GURL& url,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id) override {}
};

class MockSafeBrowsingUrlChecker : public SafeBrowsingUrlCheckerImpl {
 public:
  struct CallbackInfo {
    bool should_proceed = true;
    bool should_show_interstitial = false;
    bool should_delay_callback = false;
    NativeCheckUrlCallback callback;
    PerformedCheck performed_check = PerformedCheck::kHashDatabaseCheck;
  };

  MockSafeBrowsingUrlChecker(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      UnsafeResource::RenderProcessId render_process_id,
      const UnsafeResource::RenderFrameToken& render_frame_token,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id,
      bool url_real_time_lookup_enabled,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
      bool is_async_check)
      : SafeBrowsingUrlCheckerImpl(
            headers,
            load_flags,
            has_user_gesture,
            url_checker_delegate,
            web_contents_getter,
            /*weak_web_state=*/nullptr,
            render_process_id,
            render_frame_token,
            frame_tree_node_id.value(),
            navigation_id,
            url_real_time_lookup_enabled,
            can_check_db,
            can_check_high_confidence_allowlist,
            url_lookup_service_metric_suffix,
            ui_task_runner,
            url_lookup_service_on_ui,
            hash_realtime_service_on_ui,
            hash_realtime_selection,
            is_async_check,
            /*check_allowlist_before_hash_database=*/false,
            SessionID::InvalidValue()) {}

  // Returns the CallbackInfo that was previously added in |AddCallbackInfo|.
  // It will crash if |AddCallbackInfo| was not called.
  // If |should_delay_callback| was set to true, the callback will be cached.
  // The callback can be invoked manually via |RestartDelayedCallback|.
  // Otherwise, the callback will be invoked immediately.
  void CheckUrl(const GURL& url,
                const std::string& method,
                NativeCheckUrlCallback callback) override {
    ASSERT_GT(callback_infos_.size(), check_url_called_cnt_)
        << "Unexpected call to |CheckUrl|. Make sure to call |AddCallbackInfo| "
           "in advance if this call is expected.";
    CallbackInfo& callback_info = callback_infos_[check_url_called_cnt_++];
    if (callback_info.should_delay_callback) {
      callback_info.callback = std::move(callback);
    } else {
      std::move(callback).Run(
          /*proceed=*/callback_info.should_proceed,
          /*show_interstitial=*/
          callback_info.should_show_interstitial,
          /*has_post_commit_interstitial_skipped=*/false,
          callback_info.performed_check);
    }
  }

  void RestartDelayedCallback(size_t index) {
    ASSERT_GT(callback_infos_.size(), index);
    ASSERT_TRUE(callback_infos_[index].should_delay_callback);
    std::move(callback_infos_[index].callback)
        .Run(/*proceed=*/callback_infos_[index].should_proceed,
             /*show_interstitial=*/
             callback_infos_[index].should_show_interstitial,
             /*has_post_commit_interstitial_skipped=*/false,
             callback_infos_[index].performed_check);
  }

  // Informs how the callback in |CheckUrl| should be handled. The info applies
  // to the callback sent in |CheckUrl| in sequence. That is, the first info
  // added will be applied to the first call of |CheckUrl|.
  void AddCallbackInfo(
      bool should_proceed,
      bool should_show_interstitial,
      bool should_delay_callback,
      PerformedCheck performed_check = PerformedCheck::kHashDatabaseCheck) {
    CallbackInfo callback_info;
    callback_info.should_proceed = should_proceed;
    callback_info.should_show_interstitial = should_show_interstitial;
    callback_info.should_delay_callback = should_delay_callback;
    callback_info.performed_check = performed_check;
    callback_infos_.push_back(std::move(callback_info));
  }

  base::WeakPtr<MockSafeBrowsingUrlChecker> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  size_t check_url_called_cnt_ = 0;
  std::vector<CallbackInfo> callback_infos_;
  base::WeakPtrFactory<MockSafeBrowsingUrlChecker> weak_factory_{this};
};

struct WillStartRequestOptionalArgs {
  network::mojom::RequestDestination destination =
      network::mojom::RequestDestination::kDocument;
  int load_flags = 0;
};

struct SetUpTestOptionalArgs {
  bool url_real_time_lookup_enabled = false;
  bool should_sync_checker_check_allowlist = false;
};

}  // namespace

class SBBrowserUrlLoaderThrottleTestBase : public ::testing::Test {
 protected:
  SBBrowserUrlLoaderThrottleTestBase()
      : web_contents_(
            web_contents_factory_.CreateWebContents(&browser_context_)) {}

  scoped_refptr<UrlCheckerDelegate> GetUrlCheckerDelegate() {
    return url_checker_delegate_;
  }

  void SetUpTest(
      bool async_check_enabled,
      SetUpTestOptionalArgs optional_args = SetUpTestOptionalArgs()) {
    auto url_checker_delegate_getter = base::BindRepeating(
        [](SBBrowserUrlLoaderThrottleTestBase* test) {
          return test->GetUrlCheckerDelegate();
        },
        base::Unretained(this));
    EXPECT_CALL(mock_web_contents_getter_, Run())
        .WillRepeatedly(::testing::Return(web_contents_));
    ui_manager_ = base::MakeRefCounted<BaseUIManager>();
    async_check_tracker_ =
        async_check_enabled
            ? base::WrapUnique(new AsyncCheckTracker(
                  web_contents_, ui_manager_.get(),
                  optional_args.should_sync_checker_check_allowlist))
            : nullptr;
    std::optional<int64_t> navigation_id =
        async_check_enabled ? std::optional<int64_t>(1u) : std::nullopt;

    throttle_ = BrowserURLLoaderThrottle::Create(
        std::move(url_checker_delegate_getter), mock_web_contents_getter_.Get(),
        content::FrameTreeNodeId(), navigation_id,
        optional_args.url_real_time_lookup_enabled
            ? url_lookup_service_->GetWeakPtr()
            : nullptr,
        /*hash_realtime_service=*/nullptr,
        /*hash_realtime_selection=*/
        async_check_enabled
            ? hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService
            : hash_realtime_utils::HashRealTimeSelection::kNone,
        async_check_tracker_ ? async_check_tracker_->GetWeakPtr() : nullptr);

    url_checker_delegate_ = base::MakeRefCounted<MockUrlCheckerDelegate>();
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();

    std::unique_ptr<MockSafeBrowsingUrlChecker> sync_url_checker =
        std::make_unique<MockSafeBrowsingUrlChecker>(
            net::HttpRequestHeaders(), /*load_flags=*/0,
            /*has_user_gesture=*/false, url_checker_delegate_,
            mock_web_contents_getter_.Get(), UnsafeResource::kNoRenderProcessId,
            /*render_frame_token=*/std::nullopt, content::FrameTreeNodeId(),
            navigation_id, optional_args.url_real_time_lookup_enabled,
            /*can_check_db=*/true,
            /*can_check_high_confidence_allowlist=*/true,
            /*url_lookup_service_metric_suffix=*/"",
            /*ui_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
            /*url_lookup_service_on_ui=*/nullptr,
            /*hash_realtime_service_on_ui=*/nullptr,
            /*hash_realtime_selection=*/
            hash_realtime_utils::HashRealTimeSelection::kNone,
            /*is_async_check=*/false);
    sync_url_checker_ = sync_url_checker->GetWeakPtr();
    throttle_->SetOnSyncSBCheckerCreatedCallbackForTesting(base::BindOnce(
        &SBBrowserUrlLoaderThrottleTestBase::SetSyncUrlCheckerForTesting,
        base::Unretained(this), std::move(sync_url_checker)));
    if (async_check_enabled) {
      std::unique_ptr<MockSafeBrowsingUrlChecker> async_url_checker =
          std::make_unique<MockSafeBrowsingUrlChecker>(
              net::HttpRequestHeaders(), /*load_flags=*/0,
              /*has_user_gesture=*/false, url_checker_delegate_,
              mock_web_contents_getter_.Get(),
              UnsafeResource::kNoRenderProcessId,
              /*render_frame_token=*/std::nullopt, content::FrameTreeNodeId(),
              /*navigation_id=*/0, optional_args.url_real_time_lookup_enabled,
              /*can_check_db=*/true,
              /*can_check_high_confidence_allowlist=*/true,
              /*url_lookup_service_metric_suffix=*/"",
              /*ui_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
              /*url_lookup_service_on_ui=*/nullptr,
              /*hash_realtime_service_on_ui=*/nullptr,
              /*hash_realtime_selection=*/
              hash_realtime_utils::HashRealTimeSelection::kNone,
              /*is_async_check=*/true);
      async_url_checker_ = async_url_checker->GetWeakPtr();

      throttle_->SetOnAsyncSBCheckerCreatedCallbackForTesting(base::BindOnce(
          &SBBrowserUrlLoaderThrottleTestBase::SetAsyncUrlCheckerForTesting,
          base::Unretained(this), std::move(async_url_checker)));
    }
    throttle_->set_delegate(throttle_delegate_.get());

    url_ = GURL("https://example.com/");
    response_head_ = network::mojom::URLResponseHead::New();
  }

  void SetSyncUrlCheckerForTesting(
      std::unique_ptr<MockSafeBrowsingUrlChecker> url_checker) {
    throttle_->GetSyncSBCheckerForTesting()->SetUrlCheckerForTesting(
        std::move(url_checker));
  }
  void SetAsyncUrlCheckerForTesting(
      std::unique_ptr<MockSafeBrowsingUrlChecker> url_checker) {
    throttle_->GetAsyncSBCheckerForTesting()->SetUrlCheckerForTesting(
        std::move(url_checker));
  }

  void AddCallbackInfo(
      bool should_proceed,
      bool should_show_interstitial,
      bool should_delay_callback,
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check =
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck) {
    sync_url_checker_->AddCallbackInfo(should_proceed, should_show_interstitial,
                                       should_delay_callback, performed_check);
    if (async_url_checker_) {
      async_url_checker_->AddCallbackInfo(
          should_proceed, should_show_interstitial, should_delay_callback,
          performed_check);
    }
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillStartRequest(WillStartRequestOptionalArgs optional_args =
                                WillStartRequestOptionalArgs()) {
    bool defer = false;
    network::ResourceRequest request;
    request.url = url_;
    request.destination = optional_args.destination;
    request.load_flags = optional_args.load_flags;
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

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  GURL url_;
  network::mojom::URLResponseHeadPtr response_head_;
  std::unique_ptr<BrowserURLLoaderThrottle> throttle_;
  // Owned by |throttle_|. May be deleted before the test completes. Prefer
  // setting it up at the start of the test.
  base::WeakPtr<MockSafeBrowsingUrlChecker> sync_url_checker_;
  base::WeakPtr<MockSafeBrowsingUrlChecker> async_url_checker_;
  std::unique_ptr<FakeRealTimeUrlLookupService> url_lookup_service_ =
      std::make_unique<FakeRealTimeUrlLookupService>();
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
  std::unique_ptr<AsyncCheckTracker> async_check_tracker_;
  scoped_refptr<BaseUIManager> ui_manager_;
  content::TestBrowserContext browser_context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
      mock_web_contents_getter_;
  base::HistogramTester histogram_tester_;
};

class SBBrowserUrlLoaderThrottleTest
    : public SBBrowserUrlLoaderThrottleTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpTest() {
    bool async_check_enabled = GetParam();
    SBBrowserUrlLoaderThrottleTestBase::SetUpTest(async_check_enabled);
  }

  void RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
      bool url_real_time_lookup_enabled,
      std::string expected_histogram) {
    bool async_check_enabled = GetParam();
    SBBrowserUrlLoaderThrottleTestBase::SetUpTest(
        async_check_enabled,
        /*optional_args=*/{.url_real_time_lookup_enabled =
                               url_real_time_lookup_enabled});
    base::HistogramTester histograms;
    AddCallbackInfo(/*should_proceed=*/true,
                    /*should_show_interstitial=*/false,
                    /*should_delay_callback=*/true, performed_check);

    CallWillStartRequest();
    CallWillProcessResponse();
    task_environment_.FastForwardBy(base::Milliseconds(200));
    sync_url_checker_->RestartDelayedCallback(/*index=*/0);
    if (async_url_checker_) {
      async_url_checker_->RestartDelayedCallback(/*index=*/0);
    }
    task_environment_.RunUntilIdle();

    histograms.ExpectUniqueTimeSample(expected_histogram,
                                      base::Milliseconds(200), 1);
  }
};

INSTANTIATE_TEST_SUITE_P(AsyncCheckEnabled,
                         SBBrowserUrlLoaderThrottleTest,
                         ::testing::Bool());

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferOnSafeDocumentUrl) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), 0);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
}

TEST_P(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnUnsafeDocumentUrl) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/false,
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

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferOnUnsafeIframeUrl) {
  SetUpTest();
  // Do not call |AddSyncCallbackInfo| or |AddAsyncCallbackInfo|, so that if
  // CheckUrl is called for either (incorrectly), the test will fail.

  bool defer = CallWillStartRequest(
      {.destination = network::mojom::RequestDestination::kIframe});
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), 0);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
}

TEST_P(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferIfRedirectUrlIsUnsafe) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);
  AddCallbackInfo(/*should_proceed=*/false,
                  /*should_show_interstitial=*/true,
                  /*should_delay_callback=*/false);

  CallWillStartRequest();
  CallWillRedirectRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

TEST_P(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnSkippedUrl) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/false,
                  /*should_show_interstitial=*/true,
                  /*should_delay_callback=*/false);
  url_checker_delegate_->EnableSkipRequestCheck();

  CallWillStartRequest(
      {.destination = network::mojom::RequestDestination::kEmpty});

  bool defer = CallWillProcessResponse();
  // The loader is not deferred because the check has skipped.
  EXPECT_FALSE(defer);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferOnSkippedDocumentUrl) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/false,
                  /*should_show_interstitial=*/true,
                  /*should_delay_callback=*/false);
  url_checker_delegate_->EnableSkipRequestCheck();

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  // The loader is not deferred because the check has skipped.
  EXPECT_FALSE(defer);

  // Reset throttle to test histogram below, which only logs on destruct.
  throttle_.reset();
  // No histogram is logged because it's a skipped check (or because async
  // checks are not eligible).
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.IsAsyncCheckFasterThanSyncCheck",
      /*expected_count=*/0);
}

TEST_P(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnKnownSafeUrl) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);
  AddCallbackInfo(/*should_proceed=*/true,
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

TEST_P(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnSlowCheck) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  // Deferred because the check has not completed.
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());
}

TEST_P(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnSlowRedirectCheck) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillRedirectRequest();

  bool defer = CallWillProcessResponse();
  // Deferred because the check for redirect URL has not completed.
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  sync_url_checker_->RestartDelayedCallback(/*index=*/1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotResumeOnSlowCheckNotProceed) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/false,
                  /*should_show_interstitial=*/true,
                  /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  // Resume should not be called because the slow check returns don't proceed.
  EXPECT_FALSE(throttle_delegate_->IsResumed());
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferRedirectOnSlowCheck) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);

  CallWillStartRequest();

  bool defer = CallWillRedirectRequest();
  // Although the first check has not completed, redirect should not be
  // deferred.
  EXPECT_FALSE(defer);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DeferRedirectWhenFirstUrlAlreadyBlocked) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/false,
                  /*should_show_interstitial=*/true,
                  /*should_delay_callback=*/false);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillRedirectRequest();
  EXPECT_TRUE(defer);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyErrorCodeWhenInterstitialNotShown) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/false,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_ABORTED);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_FastCheckFromNetwork) {
  SetUpTest();
  base::HistogramTester histograms;
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);

  CallWillStartRequest();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  CallWillProcessResponse();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache", 0);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_FastCheckFromCache) {
  SetUpTest();
  base::HistogramTester histograms;
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);

  CallWillStartRequest();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  CallWillProcessResponseFromCache();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork", 0);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SlowCheckFromNetwork) {
  SetUpTest();
  base::HistogramTester histograms;
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillProcessResponse();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache", 0);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SlowCheckFromCache) {
  SetUpTest();
  base::HistogramTester histograms;
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillProcessResponseFromCache();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork", 0);
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_HashPrefixDatabaseCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
      /*url_real_time_lookup_enabled=*/false,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck");
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_HashPrefixRealTimeCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck,
      /*url_real_time_lookup_enabled=*/false,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixRealTimeCheck");
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SkippedCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped,
      /*url_real_time_lookup_enabled=*/false,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.SkippedCheck");
}

TEST_P(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_UrlRealTimeCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
      /*url_real_time_lookup_enabled=*/true,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.MockFullUrlLookup");
}

class SBBrowserUrlLoaderThrottleAsyncCheckTest
    : public SBBrowserUrlLoaderThrottleTestBase {
 protected:
  void SetUpTest(
      SetUpTestOptionalArgs optional_args = SetUpTestOptionalArgs()) {
    SBBrowserUrlLoaderThrottleTestBase::SetUpTest(/*async_check_enabled=*/true,
                                                  optional_args);
  }

  void AddSyncCallbackInfo(bool should_proceed, bool should_delay_callback) {
    sync_url_checker_->AddCallbackInfo(
        should_proceed,
        /*should_show_interstitial=*/!should_proceed,
        /*should_delay_callback=*/should_delay_callback);
  }

  void AddAsyncCallbackInfo(bool should_proceed, bool should_delay_callback) {
    async_url_checker_->AddCallbackInfo(
        should_proceed,
        /*should_show_interstitial=*/!should_proceed,
        /*should_delay_callback=*/should_delay_callback);
  }

  void VerifyHistograms(std::optional<bool> is_async_check_faster,
                        std::optional<bool> is_async_check_transferred) {
    if (is_async_check_faster.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.BrowserThrottle.IsAsyncCheckFasterThanSyncCheck",
          /*sample=*/is_async_check_faster.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.BrowserThrottle.IsAsyncCheckFasterThanSyncCheck",
          /*expected_count=*/0);
    }
    if (is_async_check_transferred.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SafeBrowsing.BrowserThrottle.IsAsyncCheckerTransferred",
          /*sample=*/is_async_check_transferred.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "SafeBrowsing.BrowserThrottle.IsAsyncCheckerTransferred",
          /*expected_count=*/0);
    }
  }
};

TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest, VerifyCheckerParams) {
  SetUpTest();
  EXPECT_EQ(throttle_->GetSyncSBCheckerForTesting(), nullptr);
  EXPECT_EQ(throttle_->GetAsyncSBCheckerForTesting(), nullptr);
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);
  CallWillStartRequest();
  EXPECT_FALSE(
      throttle_->GetSyncSBCheckerForTesting()->IsRealTimeCheckForTesting());
  EXPECT_FALSE(
      throttle_->GetSyncSBCheckerForTesting()->IsAsyncCheckForTesting());
  EXPECT_FALSE(throttle_->GetSyncSBCheckerForTesting()
                   ->IsCheckAllowlistBeforeHashDatabaseForTesting());
  EXPECT_TRUE(
      throttle_->GetAsyncSBCheckerForTesting()->IsRealTimeCheckForTesting());
  EXPECT_TRUE(
      throttle_->GetAsyncSBCheckerForTesting()->IsAsyncCheckForTesting());
  EXPECT_FALSE(throttle_->GetSyncSBCheckerForTesting()
                   ->IsCheckAllowlistBeforeHashDatabaseForTesting());
}

TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyCheckerParams_WithSyncCheckerCheckAllowlistEnabled) {
  SetUpTest(/*optional_args=*/{.should_sync_checker_check_allowlist = true});
  EXPECT_EQ(throttle_->GetSyncSBCheckerForTesting(), nullptr);
  EXPECT_EQ(throttle_->GetAsyncSBCheckerForTesting(), nullptr);
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/false);
  CallWillStartRequest();
  EXPECT_TRUE(throttle_->GetSyncSBCheckerForTesting()
                  ->IsCheckAllowlistBeforeHashDatabaseForTesting());
  EXPECT_FALSE(throttle_->GetAsyncSBCheckerForTesting()
                   ->IsCheckAllowlistBeforeHashDatabaseForTesting());
}

// Sync check completed -> WillProcessResponse called -> async check completed.
TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest, VerifyDefer_AsyncNotDeferred) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/false);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/true);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
  // Async check is transferred to the tracker.
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 1u);

  // Async check completed afterwards doesn't cause a crash.
  EXPECT_FALSE(async_url_checker_.WasInvalidated());
  async_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/false,
                   /*is_async_check_transferred=*/true);
}

// WillProcessResponse called -> Sync check completed.
TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest, VerifyDefer_SyncResumed) {
  SetUpTest();
  AddCallbackInfo(/*should_proceed=*/true,
                  /*should_show_interstitial=*/false,
                  /*should_delay_callback=*/true);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);

  defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  // Async check is not yet transferred to the tracker because sync check has
  // not completed.
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 0u);

  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 1u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/false,
                   /*is_async_check_transferred=*/true);
}

// Async check completed -> WillProcessResponse called -> Sync check completed.
TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyDefer_AsyncCompletedBeforeSync) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/true);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);

  defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);

  EXPECT_FALSE(sync_url_checker_.WasInvalidated());
  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(throttle_delegate_->IsResumed());
  // Async check already completed, so it is not transferred to the tracker.
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 0u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/true,
                   /*is_async_check_transferred=*/false);
}

// URL redirected -> Sync check completed -> WillProcessResponse called.
TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyDefer_AsyncRedirectCheckNotCompleted) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/false);
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/false);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/false);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillRedirectRequest();

  bool defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 1u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/false,
                   /*is_async_check_transferred=*/true);
}

// Async check completed -> Sync check completed -> WillProcessResponse called.
TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyDefer_AsyncCheckCompletedBeforeSyncCheck) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/true);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/true);

  CallWillStartRequest();
  async_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  bool defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 0u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/true,
                   /*is_async_check_transferred=*/false);
}

// WillProcessResponse called -> Async check completed -> Sync check completed.
TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyDefer_AsyncCheckCompletedAfterProcessResponseBeforeSyncCheck) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/true);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);

  async_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/true,
                   /*is_async_check_transferred=*/false);
}

TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyErrorCode_SyncCheckBlocked) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/false,
                      /*should_delay_callback=*/false);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/true);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  // Although async check has not completed, it should be deleted because the
  // loader is already blocked by sync check.
  EXPECT_TRUE(async_url_checker_.WasInvalidated());
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 0u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/false,
                   /*is_async_check_transferred=*/std::nullopt);
}

TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyErrorCode_SyncCheckBlockedOnRedirect) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/false);
  AddSyncCallbackInfo(/*should_proceed=*/false,
                      /*should_delay_callback=*/false);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/false);
  AddAsyncCallbackInfo(
      /*should_proceed=*/true,
      /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillRedirectRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  // Although async check has not completed, it should be deleted because the
  // loader is already blocked by sync check.
  EXPECT_TRUE(async_url_checker_.WasInvalidated());
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 0u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/false,
                   /*is_async_check_transferred=*/std::nullopt);
}

TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyErrorCode_AsyncCheckBlocked) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/true);
  AddAsyncCallbackInfo(
      /*should_proceed=*/false,
      /*should_delay_callback=*/false);

  CallWillStartRequest();
  // Once async check completes, it should not wait for sync check to block the
  // loader.
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_TRUE(sync_url_checker_.WasInvalidated());

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  EXPECT_EQ(async_check_tracker_->PendingCheckersSizeForTesting(), 0u);

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/true,
                   /*is_async_check_transferred=*/std::nullopt);
}

TEST_F(SBBrowserUrlLoaderThrottleAsyncCheckTest,
       VerifyAsyncChecksNotEligible_LoadPrefetch) {
  SetUpTest();
  AddSyncCallbackInfo(/*should_proceed=*/true,
                      /*should_delay_callback=*/true);
  // Do not call |AddAsyncCallbackInfo| so that if the async callback is
  // called (incorrectly), the test will fail.

  CallWillStartRequest({.load_flags = net::LOAD_PREFETCH});
  CallWillProcessResponse();

  sync_url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());

  // Reset throttle to test histogram below, some of which only logs on
  // destruct.
  throttle_.reset();
  VerifyHistograms(/*is_async_check_faster=*/std::nullopt,
                   /*is_async_check_transferred=*/std::nullopt);
}

}  // namespace safe_browsing

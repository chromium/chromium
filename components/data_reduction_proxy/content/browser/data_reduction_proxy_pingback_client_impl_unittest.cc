// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_pingback_client_impl.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "content/public/common/child_process_host.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

static const char kHistogramSucceeded[] =
    "DataReductionProxy.Pingback.Succeeded";
static const char kHistogramAttempted[] =
    "DataReductionProxy.Pingback.Attempted";
static const char kSessionKey[] = "fake-session";
static const char kFakeURL[] = "http://www.google.com/";
static const int64_t kBytes = 10000;
static const int64_t kBytesOriginal = 1000000;
static const int64_t kTotalPageSizeBytes = 20000;
static const float kCachedFraction = 0.5;
static const int kCrashProcessId = 1;
static const int64_t kRendererMemory = 1024;
static const int64_t kTouchCount = 10;
static const int64_t kScrollCount = 20;
static const int kNumRequestInfo = 2;
static const DataReductionProxyData::RequestInfo first_request_info(
    DataReductionProxyData::RequestInfo::Protocol::HTTP,
    false,
    base::TimeDelta::FromMilliseconds(1000),
    base::TimeDelta::FromMilliseconds(1100),
    base::TimeDelta::FromMilliseconds(1200));
static const DataReductionProxyData::RequestInfo second_request_info(
    DataReductionProxyData::RequestInfo::Protocol::HTTPS,
    true,
    base::TimeDelta::FromMilliseconds(1300),
    base::TimeDelta::FromMilliseconds(1400),
    base::TimeDelta::FromMilliseconds(1500));

}  // namespace

// Controls whether a pingback is sent or not.
class TestDataReductionProxyPingbackClientImpl
    : public DataReductionProxyPingbackClientImpl {
 public:
  TestDataReductionProxyPingbackClientImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner)
      : DataReductionProxyPingbackClientImpl(url_loader_factory,
                                             std::move(thread_task_runner)),
        should_override_random_(false),
        override_value_(0.0f),
        current_time_(base::Time::Now()) {}

  // Overrides the bahvior of the random float generator in
  // DataReductionProxyPingbackClientImpl.
  // If |should_override_random| is true, the typically random value that is
  // compared with reporting fraction will deterministically be
  // |override_value|.
  void OverrideRandom(bool should_override_random, float override_value) {
    should_override_random_ = should_override_random;
    override_value_ = override_value;
  }

  // Sets the time used for the metrics reporting time.
  void set_current_time(base::Time current_time) {
    current_time_ = current_time;
  }

 private:
  float GenerateRandomFloat() const override {
    if (should_override_random_)
      return override_value_;
    return DataReductionProxyPingbackClientImpl::GenerateRandomFloat();
  }

  base::Time CurrentTime() const override { return current_time_; }

  bool should_override_random_;
  float override_value_;
  base::Time current_time_;
};

class DataReductionProxyPingbackClientImplTest : public testing::Test {
 public:
  DataReductionProxyPingbackClientImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::ASYNC) {}

  TestDataReductionProxyPingbackClientImpl* pingback_client() const {
    return pingback_client_.get();
  }

  GURL pingback_url() { return util::AddApiKeyToUrl(params::GetPingbackURL()); }

  void Init() {
    factory()->AddResponse(pingback_url().spec(), "");
    num_network_requests_ = 0;
    factory()->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          intercepted_url_ = request.url;
          intercepted_headers_ = request.headers;
          intercepted_body_ = network::GetUploadData(request);
          ++num_network_requests_;
        }));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    pingback_client_ =
        std::make_unique<TestDataReductionProxyPingbackClientImpl>(
            test_shared_loader_factory_,
            scoped_task_environment_.GetMainThreadTaskRunner());
    page_id_ = 0u;
  }

  void CreateAndSendPingback(bool lofi_received,
                             bool client_lofi_requested,
                             bool lite_page_received,
                             bool app_background_occurred,
                             bool opt_out_occurred,
                             bool crash,
                             bool black_listed) {
    timing_ = std::make_unique<DataReductionProxyPageLoadTiming>(
        base::Time::FromJsTime(1500) /* navigation_start */,
        base::Optional<base::TimeDelta>(
            base::TimeDelta::FromMilliseconds(1600)) /* response_start */,
        base::Optional<base::TimeDelta>(
            base::TimeDelta::FromMilliseconds(1700)) /* load_event_start */,
        base::Optional<base::TimeDelta>(
            base::TimeDelta::FromMilliseconds(1800)) /* first_image_paint */,
        base::Optional<base::TimeDelta>(base::TimeDelta::FromMilliseconds(
            1900)) /* first_contentful_paint */,
        base::Optional<base::TimeDelta>(base::TimeDelta::FromMilliseconds(
            2000)) /* experimental_first_meaningful_paint */,
        base::Optional<base::TimeDelta>(
            base::TimeDelta::FromMilliseconds(3000)) /* first_input_delay */,
        base::Optional<base::TimeDelta>(base::TimeDelta::FromMilliseconds(
            100)) /* parse_blocked_on_script_load_duration */,
        base::Optional<base::TimeDelta>(
            base::TimeDelta::FromMilliseconds(2000)) /* parse_stop */,
        base::Optional<base::TimeDelta>(
            base::TimeDelta::FromMilliseconds(5000)) /* page_end_time */,
        kBytes /* network_bytes */, kBytesOriginal /* original_network_bytes */,
        kTotalPageSizeBytes /* total_page_size_bytes */,
        kCachedFraction /* cached_fraction */, app_background_occurred,
        opt_out_occurred, kRendererMemory,
        crash ? kCrashProcessId : content::ChildProcessHost::kInvalidUniqueID,
        PageloadMetrics_PageEndReason_END_NONE, kTouchCount /* touch_count */,
        kScrollCount /* scroll_count */);

    DataReductionProxyData request_data;
    request_data.set_session_key(kSessionKey);
    request_data.set_request_url(GURL(kFakeURL));
    request_data.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    request_data.set_connection_type(
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
    request_data.set_lofi_received(lofi_received);
    request_data.set_black_listed(black_listed);
    request_data.set_client_lofi_requested(client_lofi_requested);
    request_data.set_lite_page_received(lite_page_received);
    request_data.set_page_id(page_id_);
    request_data.add_request_info(first_request_info);
    request_data.add_request_info(second_request_info);
    static_cast<DataReductionProxyPingbackClient*>(pingback_client())
        ->SendPingback(request_data, *timing_);
    page_id_++;
  }

  void WaitForPingbackResponse() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Send a fake crash report from crash_reporter.
  void ReportCrash(bool oom) {
#if defined(OS_ANDROID)
    crash_reporter::ChildExitObserver::TerminationInfo info;
    crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet types(
        {oom ? crash_reporter::CrashMetricsReporter::ProcessedCrashCounts::
                   kRendererForegroundVisibleOom
             : crash_reporter::CrashMetricsReporter::ProcessedCrashCounts::
                   kRendererForegroundVisibleCrash});
    static_cast<crash_reporter::CrashMetricsReporter::Observer*>(
        pingback_client_.get())
        ->OnCrashDumpProcessed(kCrashProcessId, types);
#endif
  }

  network::TestURLLoaderFactory* factory() { return &test_url_loader_factory_; }

  const DataReductionProxyPageLoadTiming& timing() { return *timing_; }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  uint64_t page_id() const { return page_id_; }

  GURL intercepted_url() { return intercepted_url_; }

  std::string upload_content_type() {
    std::string content_type;
    intercepted_headers_.GetHeader(net::HttpRequestHeaders::kContentType,
                                   &content_type);
    return content_type;
  }

  std::string upload_data() { return intercepted_body_; }

  int num_network_requests() { return num_network_requests_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<TestDataReductionProxyPingbackClientImpl> pingback_client_;
  std::unique_ptr<DataReductionProxyPageLoadTiming> timing_;
  base::HistogramTester histogram_tester_;
  uint64_t page_id_;
  GURL intercepted_url_;
  net::HttpRequestHeaders intercepted_headers_;
  std::string intercepted_body_;
  int num_network_requests_;
};

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyPingbackContent) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  uint64_t data_page_id = page_id();
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  EXPECT_EQ(current_time, protobuf_parser::TimestampToTime(
                              batched_request.metrics_sent_time()));
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(
      timing().navigation_start,
      protobuf_parser::TimestampToTime(pageload_metrics.first_request_time()));
  EXPECT_EQ(timing().response_start.value(),
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_byte()));
  EXPECT_EQ(
      timing().load_event_start.value(),
      protobuf_parser::DurationToTimeDelta(pageload_metrics.page_load_time()));
  EXPECT_EQ(timing().first_image_paint.value(),
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_image_paint()));
  EXPECT_EQ(timing().first_contentful_paint.value(),
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_contentful_paint()));
  EXPECT_EQ(
      timing().experimental_first_meaningful_paint.value(),
      protobuf_parser::DurationToTimeDelta(
          pageload_metrics.experimental_time_to_first_meaningful_paint()));
  EXPECT_EQ(timing().first_input_delay.value(),
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.first_input_delay()));
  EXPECT_EQ(timing().parse_blocked_on_script_load_duration.value(),
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.parse_blocked_on_script_load_duration()));
  EXPECT_EQ(timing().parse_stop.value(), protobuf_parser::DurationToTimeDelta(
                                             pageload_metrics.parse_stop()));
  EXPECT_EQ(
      timing().page_end_time.value(),
      protobuf_parser::DurationToTimeDelta(pageload_metrics.page_end_time()));

  EXPECT_EQ(kSessionKey, pageload_metrics.session_key());
  EXPECT_EQ(kFakeURL, pageload_metrics.first_request_url());
  EXPECT_EQ(kBytes, pageload_metrics.compressed_page_size_bytes());
  EXPECT_EQ(kBytesOriginal, pageload_metrics.original_page_size_bytes());
  EXPECT_EQ(kTotalPageSizeBytes, pageload_metrics.total_page_size_bytes());
  EXPECT_EQ(kTouchCount, pageload_metrics.touch_count());
  EXPECT_EQ(kScrollCount, pageload_metrics.scroll_count());
  EXPECT_EQ(kCachedFraction, pageload_metrics.cached_fraction());
  EXPECT_EQ(data_page_id, pageload_metrics.page_id());
  EXPECT_EQ(kNumRequestInfo,
            pageload_metrics.main_frame_network_request_size());
  EXPECT_EQ(protobuf_parser::ProtoRequestInfoProtocolFromRequestInfoProtocol(
                first_request_info.protocol),
            pageload_metrics.main_frame_network_request(0).protocol());
  EXPECT_EQ(first_request_info.proxy_bypass,
            pageload_metrics.main_frame_network_request(0).proxy_bypass());
  EXPECT_EQ(first_request_info.dns_time,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.main_frame_network_request(0).dns_time()));
  EXPECT_EQ(first_request_info.connect_time,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.main_frame_network_request(0).connect_time()));
  EXPECT_EQ(first_request_info.http_time,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.main_frame_network_request(0).http_time()));

  EXPECT_EQ(protobuf_parser::ProtoRequestInfoProtocolFromRequestInfoProtocol(
                second_request_info.protocol),
            pageload_metrics.main_frame_network_request(1).protocol());
  EXPECT_EQ(second_request_info.proxy_bypass,
            pageload_metrics.main_frame_network_request(1).proxy_bypass());
  EXPECT_EQ(second_request_info.dns_time,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.main_frame_network_request(1).dns_time()));
  EXPECT_EQ(second_request_info.connect_time,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.main_frame_network_request(1).connect_time()));
  EXPECT_EQ(second_request_info.http_time,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.main_frame_network_request(1).http_time()));

  EXPECT_EQ(PageloadMetrics_PreviewsType_NONE,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_UNKNOWN,
            pageload_metrics.previews_opt_out());

  EXPECT_EQ(
      PageloadMetrics_EffectiveConnectionType_EFFECTIVE_CONNECTION_TYPE_OFFLINE,
      pageload_metrics.effective_connection_type());
  EXPECT_EQ(PageloadMetrics_ConnectionType_CONNECTION_UNKNOWN,
            pageload_metrics.connection_type());
  EXPECT_EQ(PageloadMetrics_PageEndReason_END_NONE,
            pageload_metrics.page_end_reason());
  EXPECT_EQ(kRendererMemory, pageload_metrics.renderer_memory_usage_kb());
  EXPECT_EQ(std::string(), pageload_metrics.holdback_group());
  EXPECT_EQ(PageloadMetrics_RendererCrashType_NO_CRASH,
            pageload_metrics.renderer_crash_type());
  EXPECT_EQ(base::SysInfo::AmountOfPhysicalMemory() / 1024,
            batched_request.device_info().total_device_memory_kb());

  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyHoldback) {
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ("Enabled", pageload_metrics.holdback_group());
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
}

TEST_F(DataReductionProxyPingbackClientImplTest,
       VerifyTwoPingbacksBatchedContent) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  // First pingback
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);

  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  // Two more pingbacks batched together.
  std::list<uint64_t> page_ids;
  page_ids.push_back(page_id());
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 2);
  page_ids.push_back(page_id());
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 3);
  EXPECT_EQ(num_network_requests(), 1);

  WaitForPingbackResponse();

  // Check the state of the second pingback.
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 2);
  EXPECT_EQ(current_time, protobuf_parser::TimestampToTime(
                              batched_request.metrics_sent_time()));
  EXPECT_EQ(base::SysInfo::AmountOfPhysicalMemory() / 1024,
            batched_request.device_info().total_device_memory_kb());

  // Verify the content of both pingbacks.
  for (size_t i = 0; i < 2; ++i) {
    PageloadMetrics pageload_metrics = batched_request.pageloads(i);
    EXPECT_EQ(timing().navigation_start,
              protobuf_parser::TimestampToTime(
                  pageload_metrics.first_request_time()));
    EXPECT_EQ(timing().response_start.value(),
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.time_to_first_byte()));
    EXPECT_EQ(timing().load_event_start.value(),
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.page_load_time()));
    EXPECT_EQ(timing().first_image_paint.value(),
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.time_to_first_image_paint()));
    EXPECT_EQ(timing().first_contentful_paint.value(),
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.time_to_first_contentful_paint()));
    EXPECT_EQ(
        timing().experimental_first_meaningful_paint.value(),
        protobuf_parser::DurationToTimeDelta(
            pageload_metrics.experimental_time_to_first_meaningful_paint()));
    EXPECT_EQ(timing().first_input_delay.value(),
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.first_input_delay()));
    EXPECT_EQ(timing().parse_blocked_on_script_load_duration.value(),
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.parse_blocked_on_script_load_duration()));
    EXPECT_EQ(timing().parse_stop.value(), protobuf_parser::DurationToTimeDelta(
                                               pageload_metrics.parse_stop()));
    EXPECT_EQ(
        timing().page_end_time.value(),
        protobuf_parser::DurationToTimeDelta(pageload_metrics.page_end_time()));

    EXPECT_EQ(kSessionKey, pageload_metrics.session_key());
    EXPECT_EQ(kFakeURL, pageload_metrics.first_request_url());
    EXPECT_EQ(kBytes, pageload_metrics.compressed_page_size_bytes());
    EXPECT_EQ(kBytesOriginal, pageload_metrics.original_page_size_bytes());
    EXPECT_EQ(kTotalPageSizeBytes, pageload_metrics.total_page_size_bytes());
    EXPECT_EQ(kTouchCount, pageload_metrics.touch_count());
    EXPECT_EQ(kScrollCount, pageload_metrics.scroll_count());
    EXPECT_EQ(kCachedFraction, pageload_metrics.cached_fraction());
    EXPECT_EQ(kNumRequestInfo,
              pageload_metrics.main_frame_network_request_size());
    EXPECT_EQ(protobuf_parser::ProtoRequestInfoProtocolFromRequestInfoProtocol(
                  first_request_info.protocol),
              pageload_metrics.main_frame_network_request(0).protocol());
    EXPECT_EQ(first_request_info.proxy_bypass,
              pageload_metrics.main_frame_network_request(0).proxy_bypass());
    EXPECT_EQ(first_request_info.dns_time,
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.main_frame_network_request(0).dns_time()));
    EXPECT_EQ(
        first_request_info.connect_time,
        protobuf_parser::DurationToTimeDelta(
            pageload_metrics.main_frame_network_request(0).connect_time()));
    EXPECT_EQ(first_request_info.http_time,
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.main_frame_network_request(0).http_time()));

    EXPECT_EQ(protobuf_parser::ProtoRequestInfoProtocolFromRequestInfoProtocol(
                  second_request_info.protocol),
              pageload_metrics.main_frame_network_request(1).protocol());
    EXPECT_EQ(second_request_info.proxy_bypass,
              pageload_metrics.main_frame_network_request(1).proxy_bypass());
    EXPECT_EQ(second_request_info.dns_time,
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.main_frame_network_request(1).dns_time()));
    EXPECT_EQ(
        second_request_info.connect_time,
        protobuf_parser::DurationToTimeDelta(
            pageload_metrics.main_frame_network_request(1).connect_time()));
    EXPECT_EQ(second_request_info.http_time,
              protobuf_parser::DurationToTimeDelta(
                  pageload_metrics.main_frame_network_request(1).http_time()));

    EXPECT_EQ(page_ids.front(), pageload_metrics.page_id());
    page_ids.pop_front();
    EXPECT_EQ(
        PageloadMetrics_EffectiveConnectionType_EFFECTIVE_CONNECTION_TYPE_OFFLINE,
        pageload_metrics.effective_connection_type());
    EXPECT_EQ(PageloadMetrics_ConnectionType_CONNECTION_UNKNOWN,
              pageload_metrics.connection_type());
    EXPECT_EQ(PageloadMetrics_PageEndReason_END_NONE,
              pageload_metrics.page_end_reason());
    EXPECT_EQ(kRendererMemory, pageload_metrics.renderer_memory_usage_kb());
  }

  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 3);
}

TEST_F(DataReductionProxyPingbackClientImplTest, SendTwoPingbacks) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 2);
  EXPECT_EQ(num_network_requests(), 1);

  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 2);
  histogram_tester().ExpectTotalCount(kHistogramAttempted, 2);
}

TEST_F(DataReductionProxyPingbackClientImplTest, NoPingbackSent) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(0.0f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, false, 1);
  histogram_tester().ExpectTotalCount(kHistogramSucceeded, 0);
  EXPECT_EQ(num_network_requests(), 0);
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyReportingBehvaior) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);

  // Verify that if the random number is less than the reporting fraction, the
  // pingback is created.
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(0.5f);
  pingback_client()->OverrideRandom(true, 0.4f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(num_network_requests(), 1);
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);

  // Verify that if the random number is greater than the reporting fraction,
  // the pingback is not created.
  pingback_client()->OverrideRandom(true, 0.6f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectBucketCount(kHistogramAttempted, false, 1);
  EXPECT_EQ(num_network_requests(), 1);

  // Verify that if the random number is equal to the reporting fraction, the
  // pingback is not created. Specifically, if the reporting fraction is zero,
  // and the random number is zero, no pingback is sent.
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(0.0f);
  pingback_client()->OverrideRandom(true, 0.0f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectBucketCount(kHistogramAttempted, false, 2);
  EXPECT_EQ(num_network_requests(), 1);

  // Verify that the command line flag forces a pingback.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyForcePingback);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(0.0f);
  pingback_client()->OverrideRandom(true, 1.0f);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectBucketCount(kHistogramAttempted, true, 2);
  EXPECT_EQ(num_network_requests(), 2);
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 2);
}

TEST_F(DataReductionProxyPingbackClientImplTest, FailedPingback) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  // Simulate a network error.
  factory()->ClearResponses();
  factory()->AddResponse(pingback_url().spec(), "", net::HTTP_UNAUTHORIZED);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(num_network_requests(), 1);
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, false, 1);
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyLoFiContentNoOptOut) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      true /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LOFI,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_NON_OPT_OUT,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyLoFiContentOptOut) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      true /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      true /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LOFI,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_OPT_OUT,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest,
       VerifyClientLoFiContentOptOut) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      false /* lofi_received */, true /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      true /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LOFI,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_OPT_OUT,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyLoFiContentBackground) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      true /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, true /* app_background_occurred */,
      true /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LOFI,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_UNKNOWN,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyBlackListContent) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, false /* renderer_crash */,
      true /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_CLIENT_BLACKLIST_PREVENTED_PREVIEW,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_UNKNOWN,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyLitePageContent) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      true /* lite_page_received */, false /* app_background_occurred */,
      true /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LITE_PAGE,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_OPT_OUT,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyTwoLitePagePingbacks) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);
  base::Time current_time = base::Time::UnixEpoch();
  pingback_client()->set_current_time(current_time);
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      true /* lite_page_received */, false /* app_background_occurred */,
      true /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  EXPECT_EQ(num_network_requests(), 1);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LITE_PAGE,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_OPT_OUT,
            pageload_metrics.previews_opt_out());
  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      true /* lite_page_received */, false /* app_background_occurred */,
      true /* opt_out_occurred */, false /* renderer_crash */,
      false /* black_listed */);
  histogram_tester().ExpectUniqueSample(kHistogramAttempted, true, 2);
  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_PreviewsType_LITE_PAGE,
            pageload_metrics.previews_type());
  EXPECT_EQ(PageloadMetrics_PreviewsOptOut_OPT_OUT,
            pageload_metrics.previews_opt_out());
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyCrashOomBehavior) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);

  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, true /* renderer_crash */,
      false /* black_listed */);

  ReportCrash(true /* oom */);

  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
#if defined(OS_ANDROID)
  EXPECT_EQ(PageloadMetrics_RendererCrashType_ANDROID_FOREGROUND_OOM,
            pageload_metrics.renderer_crash_type());
#else
  EXPECT_EQ(PageloadMetrics_RendererCrashType_NOT_ANALYZED,
            pageload_metrics.renderer_crash_type());
#endif
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
}

TEST_F(DataReductionProxyPingbackClientImplTest, VerifyCrashNotOomBehavior) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);

  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, true /* renderer_crash */,
      false /* black_listed */);

  ReportCrash(false /* oom */);

  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
#if defined(OS_ANDROID)
  EXPECT_EQ(PageloadMetrics_RendererCrashType_OTHER_CRASH,
            pageload_metrics.renderer_crash_type());
#else
  EXPECT_EQ(PageloadMetrics_RendererCrashType_NOT_ANALYZED,
            pageload_metrics.renderer_crash_type());
#endif
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
}

TEST_F(DataReductionProxyPingbackClientImplTest,
       VerifyCrashNotAnalyzedBehavior) {
  Init();
  EXPECT_EQ(num_network_requests(), 0);
  pingback_client()->OverrideRandom(true, 0.5f);
  static_cast<DataReductionProxyPingbackClient*>(pingback_client())
      ->SetPingbackReportingFraction(1.0f);

  CreateAndSendPingback(
      false /* lofi_received */, false /* client_lofi_requested */,
      false /* lite_page_received */, false /* app_background_occurred */,
      false /* opt_out_occurred */, true /* renderer_crash */,
      false /* black_listed */);

  // Don't report the crash dump details.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));

  EXPECT_EQ(upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(upload_data());
  EXPECT_EQ(batched_request.pageloads_size(), 1);
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(PageloadMetrics_RendererCrashType_NOT_ANALYZED,
            pageload_metrics.renderer_crash_type());
  WaitForPingbackResponse();
  histogram_tester().ExpectUniqueSample(kHistogramSucceeded, true, 1);
}

}  // namespace data_reduction_proxy

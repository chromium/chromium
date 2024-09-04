// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/domain_reliability/context.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/test_util.h"
#include "components/domain_reliability/uploader.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

using base::Value;

typedef std::vector<const DomainReliabilityBeacon*> BeaconVector;

const char kBeaconOutcomeHistogram[] = "Net.DomainReliability.BeaconOutcome";

std::unique_ptr<DomainReliabilityBeacon> MakeCustomizedBeacon(
    MockableTime* time,
    std::string status,
    std::string quic_error,
    bool quic_port_migration_detected) {
  std::unique_ptr<DomainReliabilityBeacon> beacon(
      new DomainReliabilityBeacon());
  beacon->url = GURL("https://localhost/");
  beacon->status = status;
  beacon->quic_error = quic_error;
  beacon->chrome_error = net::ERR_CONNECTION_RESET;
  beacon->server_ip = "127.0.0.1";
  beacon->was_proxied = false;
  beacon->protocol = "HTTP";
  beacon->details.quic_broken = true;
  beacon->details.quic_port_migration_detected = quic_port_migration_detected;
  beacon->http_response_code = -1;
  beacon->elapsed = base::Milliseconds(250);
  beacon->start_time = time->NowTicks() - beacon->elapsed;
  beacon->upload_depth = 0;
  beacon->sample_rate = 1.0;
  beacon->isolation_info = net::IsolationInfo();
  return beacon;
}

std::unique_ptr<DomainReliabilityBeacon> MakeBeacon(MockableTime* time) {
  return MakeCustomizedBeacon(time, "tcp.connection_reset" /* status */,
                              "" /* quic_error */,
                              false /* quic_port_migration_detected */);
}

std::unique_ptr<DomainReliabilityBeacon> MakeBeaconWithIsolationInfo(
    MockableTime* time,
    const std::string& status,
    const net::IsolationInfo& isolation_info) {
  std::unique_ptr<DomainReliabilityBeacon> beacon =
      MakeCustomizedBeacon(time, status, /*quic_error=*/"",
                           /*quic_port_migration_detected=*/false);
  beacon->isolation_info = isolation_info;
  return beacon;
}

// Create a status string from in integer. For eviction tests. Include string
// values before and after the string representation of the integer, to make
// sure only exact matches are found when searching a JSON string.
std::string StatusFromInt(int i) {
  return base::StringPrintf("status%i.test", i);
}

template <typename ValueTypeFindResult,
          typename ValueType,
          ValueTypeFindResult (Value::Dict::*FindValueType)(std::string_view)
              const>
struct HasValue {
  bool operator()(const Value::Dict& dict,
                  const std::string& key,
                  ValueType expected_value) {
    ValueTypeFindResult actual_value = (dict.*FindValueType)(key);
    if (actual_value)
      EXPECT_EQ(expected_value, *actual_value);
    return actual_value && (expected_value == *actual_value);
  }
};

HasValue<std::optional<bool>, bool, &Value::Dict::FindBoolByDottedPath>
    HasBooleanValue;
HasValue<std::optional<double>, double, &Value::Dict::FindDoubleByDottedPath>
    HasDoubleValue;
HasValue<std::optional<int>, int, &Value::Dict::FindIntByDottedPath>
    HasIntegerValue;
HasValue<const std::string*, std::string, &Value::Dict::FindStringByDottedPath>
    HasStringValue;

const Value::Dict* GetEntryFromReport(const Value::Dict& report, size_t index) {
  const Value::List* entries = report.FindList("entries");
  if (!entries || index >= entries->size()) {
    return nullptr;
  }
  const Value& entry = (*entries)[index];
  if (!entry.is_dict()) {
    return nullptr;
  }
  return &entry.GetDict();
}

class DomainReliabilityContextTest : public testing::Test {
 protected:
  DomainReliabilityContextTest()
      : last_network_change_time_(time_.NowTicks()),
        dispatcher_(&time_),
        params_(MakeTestSchedulerParams()),
        uploader_(
            base::BindRepeating(&DomainReliabilityContextTest::OnUploadRequest,
                                base::Unretained(this))),
        upload_reporter_string_("test-reporter"),
        upload_allowed_callback_(base::BindRepeating(
            &DomainReliabilityContextTest::UploadAllowedCallback,
            base::Unretained(this))),
        upload_pending_(false) {
    // Make sure that the last network change does not overlap requests
    // made in test cases, which start 250ms in the past (see |MakeBeacon|).
    last_network_change_time_ = time_.NowTicks();
    time_.Advance(base::Seconds(1));
  }

  void InitContext(std::unique_ptr<const DomainReliabilityConfig> config) {
    context_ = std::make_unique<DomainReliabilityContext>(
        &time_, params_, upload_reporter_string_, &last_network_change_time_,
        upload_allowed_callback_, &dispatcher_, &uploader_, std::move(config));
  }

  void ShutDownContext() { context_.reset(); }

  base::TimeDelta min_delay() const { return params_.minimum_upload_delay; }
  base::TimeDelta max_delay() const { return params_.maximum_upload_delay; }
  base::TimeDelta retry_interval() const {
    return params_.upload_retry_interval;
  }
  base::TimeDelta zero_delta() const { return base::Microseconds(0); }

  bool upload_allowed_callback_pending() const {
    return !upload_allowed_result_callback_.is_null();
  }

  bool upload_pending() const { return upload_pending_; }

  const std::string& upload_report() const {
    EXPECT_TRUE(upload_pending_);
    return upload_report_;
  }

  int upload_max_depth() const {
    EXPECT_TRUE(upload_pending_);
    return upload_max_depth_;
  }

  const GURL& upload_url() const {
    EXPECT_TRUE(upload_pending_);
    return upload_url_;
  }

  const net::IsolationInfo& upload_isolation_info() const {
    EXPECT_TRUE(upload_pending_);
    return upload_isolation_info_;
  }

  void CallUploadCallback(DomainReliabilityUploader::UploadResult result) {
    ASSERT_TRUE(upload_pending_);
    std::move(upload_callback_).Run(result);
    upload_pending_ = false;
    ++num_uploads_completed_;
    EXPECT_EQ(num_uploads_completed_, num_uploads_);
  }

  bool CheckNoBeacons() {
    BeaconVector beacons;
    context_->GetQueuedBeaconsForTesting(&beacons);
    return beacons.empty();
  }

  const url::Origin& upload_allowed_origin() { return upload_allowed_origin_; }

  void CallUploadAllowedResultCallback(bool allowed) {
    DCHECK(!upload_allowed_result_callback_.is_null());
    std::move(upload_allowed_result_callback_).Run(allowed);
  }

  MockTime time_;
  base::TimeTicks last_network_change_time_;
  DomainReliabilityDispatcher dispatcher_;
  DomainReliabilityScheduler::Params params_;
  MockUploader uploader_;
  std::string upload_reporter_string_;
  DomainReliabilityContext::UploadAllowedCallback upload_allowed_callback_;
  std::unique_ptr<DomainReliabilityContext> context_;

 private:
  void OnUploadRequest(const std::string& report_json,
                       int max_upload_depth,
                       const GURL& upload_url,
                       const net::IsolationInfo& isolation_info,
                       DomainReliabilityUploader::UploadCallback callback) {
    EXPECT_EQ(num_uploads_completed_, num_uploads_);
    ASSERT_FALSE(upload_pending_);
    upload_report_ = report_json;
    upload_max_depth_ = max_upload_depth;
    upload_url_ = upload_url;
    upload_isolation_info_ = isolation_info;
    upload_callback_ = std::move(callback);
    upload_pending_ = true;
    ++num_uploads_;
  }

  void UploadAllowedCallback(const url::Origin& origin,
                             base::OnceCallback<void(bool)> callback) {
    upload_allowed_origin_ = origin;
    upload_allowed_result_callback_ = std::move(callback);
  }

  int num_uploads_ = 0;
  int num_uploads_completed_ = 0;

  bool upload_pending_;
  std::string upload_report_;
  int upload_max_depth_;
  GURL upload_url_;
  net::IsolationInfo upload_isolation_info_;
  DomainReliabilityUploader::UploadCallback upload_callback_;

  url::Origin upload_allowed_origin_;
  base::OnceCallback<void(bool)> upload_allowed_result_callback_;
};

TEST_F(DomainReliabilityContextTest, Create) {
  InitContext(MakeTestConfig());
  EXPECT_TRUE(CheckNoBeacons());
}

TEST_F(DomainReliabilityContextTest, QueueBeacon) {
  base::HistogramTester histograms;
  InitContext(MakeTestConfig());
  context_->OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  ShutDownContext();
  histograms.ExpectBucketCount(
      kBeaconOutcomeHistogram,
      DomainReliabilityBeacon::Outcome::kContextShutDown, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);
}

TEST_F(DomainReliabilityContextTest, MaxNestedBeaconSchedules) {
  InitContext(MakeTestConfig());
  GURL url("http://example/always_report");
  std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeacon(&time_);
  beacon->upload_depth = DomainReliabilityContext::kMaxUploadDepthToSchedule;
  context_->OnBeacon(std::move(beacon));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
}

TEST_F(DomainReliabilityContextTest, OverlyNestedBeaconDoesNotSchedule) {
  InitContext(MakeTestConfig());
  GURL url("http://example/always_report");
  std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeacon(&time_);
  beacon->upload_depth =
      DomainReliabilityContext::kMaxUploadDepthToSchedule + 1;
  context_->OnBeacon(std::move(beacon));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());
}

TEST_F(DomainReliabilityContextTest,
    MaxNestedBeaconAfterOverlyNestedBeaconSchedules) {
  base::HistogramTester histograms;
  InitContext(MakeTestConfig());
  // Add a beacon for a report that's too nested to schedule a beacon.
  std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeacon(&time_);
  beacon->upload_depth =
      DomainReliabilityContext::kMaxUploadDepthToSchedule + 1;
  context_->OnBeacon(std::move(beacon));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());

  // Add a beacon for a report that should schedule a beacon, and make sure it
  // doesn't schedule until the deadline.
  beacon = MakeBeacon(&time_);
  beacon->upload_depth = DomainReliabilityContext::kMaxUploadDepthToSchedule;
  context_->OnBeacon(std::move(beacon));

  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(2u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());

  // Check that both beacons were uploaded.
  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  histograms.ExpectBucketCount(kBeaconOutcomeHistogram,
                               DomainReliabilityBeacon::Outcome::kUploaded, 2);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 2);

  EXPECT_TRUE(CheckNoBeacons());
  ShutDownContext();
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 2);
}

TEST_F(DomainReliabilityContextTest, ReportUpload) {
  base::HistogramTester histograms;
  InitContext(MakeTestConfig());
  context_->OnBeacon(
      MakeCustomizedBeacon(&time_, "tcp.connection_reset", "", true));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  base::Value::Dict value = base::test::ParseJsonDict(upload_report());
  const Value::Dict* entry = GetEntryFromReport(value, 0);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(HasStringValue(*entry, "failure_data.custom_error",
                             "net::ERR_CONNECTION_RESET"));
  EXPECT_TRUE(HasBooleanValue(*entry, "network_changed", false));
  EXPECT_TRUE(HasStringValue(*entry, "protocol", "HTTP"));
  EXPECT_TRUE(HasBooleanValue(*entry, "quic_broken", true));
  EXPECT_TRUE(HasBooleanValue(*entry, "quic_port_migration_detected", true));
  // N.B.: Assumes max_delay is 5 minutes.
  EXPECT_TRUE(HasIntegerValue(*entry, "request_age_ms", 300250));
  EXPECT_TRUE(HasIntegerValue(*entry, "request_elapsed_ms", 250));
  EXPECT_TRUE(HasDoubleValue(*entry, "sample_rate", 1.0));
  EXPECT_TRUE(HasStringValue(*entry, "server_ip", "127.0.0.1"));
  EXPECT_TRUE(HasStringValue(*entry, "status", "tcp.connection_reset"));
  EXPECT_TRUE(HasStringValue(*entry, "url", "https://localhost/"));
  EXPECT_TRUE(HasBooleanValue(*entry, "was_proxied", false));

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  histograms.ExpectBucketCount(kBeaconOutcomeHistogram,
                               DomainReliabilityBeacon::Outcome::kUploaded, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);

  EXPECT_TRUE(CheckNoBeacons());
  ShutDownContext();
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);
}

TEST_F(DomainReliabilityContextTest, ReportUploadFails) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(
      MakeCustomizedBeacon(&time_, "tcp.connection_reset", "", true));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  // The upload fails.
  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::FAILURE;
  CallUploadCallback(result);

  // The beacon should still be pending.
  beacons.clear();
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  // Another upload should be queued.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
}

// Make sure that requests with only one underlying NetworkIsolationKey are
// uploaded at a time, in FIFO order.
TEST_F(DomainReliabilityContextTest, ReportUploadNetworkIsolationKey) {
  const auto kOrigin1 = url::Origin::Create(GURL("https://example.com"));
  const auto kSiteForCookies1 = net::SiteForCookies::FromOrigin(kOrigin1);
  const auto kIsolationInfo1 =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 kOrigin1, kOrigin1, kSiteForCookies1);
  const auto kIsolationInfo2 = net::IsolationInfo::CreateTransient();
  const auto kIsolationInfo3 = net::IsolationInfo::CreateTransient();

  InitContext(MakeTestConfig());

  // Three beacons with `kIsolationInfo1`, two with `kIsolationInfo2`, and one
  // with `kIsolationInfo3`. Have beacons with the same key both adjacent to
  // each other, and separated by beacons with other keys. Give each a unique
  // status, so it's easy to check which beacons are included in each report.
  const char kStatusNik11[] = "nik1.status1";
  const char kStatusNik12[] = "nik1.status2";
  const char kStatusNik13[] = "nik1.status3";
  const char kStatusNik21[] = "nik2.status1";
  const char kStatusNik22[] = "nik2.status2";
  const char kStatusNik31[] = "nik3.status1";
  context_->OnBeacon(
      MakeBeaconWithIsolationInfo(&time_, kStatusNik11, kIsolationInfo1));
  context_->OnBeacon(
      MakeBeaconWithIsolationInfo(&time_, kStatusNik12, kIsolationInfo1));
  context_->OnBeacon(
      MakeBeaconWithIsolationInfo(&time_, kStatusNik21, kIsolationInfo2));
  context_->OnBeacon(
      MakeBeaconWithIsolationInfo(&time_, kStatusNik31, kIsolationInfo3));
  context_->OnBeacon(
      MakeBeaconWithIsolationInfo(&time_, kStatusNik13, kIsolationInfo1));
  context_->OnBeacon(
      MakeBeaconWithIsolationInfo(&time_, kStatusNik22, kIsolationInfo2));

  // All the beacons should be queued, in FIFO order.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(6u, beacons.size());
  EXPECT_TRUE(kIsolationInfo1.IsEqualForTesting(beacons[0]->isolation_info));
  EXPECT_TRUE(kIsolationInfo1.IsEqualForTesting(beacons[1]->isolation_info));
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(beacons[2]->isolation_info));
  EXPECT_TRUE(kIsolationInfo3.IsEqualForTesting(beacons[3]->isolation_info));
  EXPECT_TRUE(kIsolationInfo1.IsEqualForTesting(beacons[4]->isolation_info));
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(beacons[5]->isolation_info));
  EXPECT_EQ(kStatusNik11, beacons[0]->status);
  EXPECT_EQ(kStatusNik12, beacons[1]->status);
  EXPECT_EQ(kStatusNik21, beacons[2]->status);
  EXPECT_EQ(kStatusNik31, beacons[3]->status);
  EXPECT_EQ(kStatusNik13, beacons[4]->status);
  EXPECT_EQ(kStatusNik22, beacons[5]->status);

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());
  EXPECT_TRUE(kIsolationInfo1.IsEqualForTesting(upload_isolation_info()));

  // Check that only the strings associated with the first NIK are present in
  // the report.
  EXPECT_NE(upload_report().find(kStatusNik11), std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik12), std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik13), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik21), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik22), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik31), std::string::npos);

  // Complete upload.
  DomainReliabilityUploader::UploadResult successful_result;
  successful_result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(successful_result);

  // There should still be 3 beacons queued, in the same order as before.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(3u, beacons.size());
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(beacons[0]->isolation_info));
  EXPECT_TRUE(kIsolationInfo3.IsEqualForTesting(beacons[1]->isolation_info));
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(beacons[2]->isolation_info));
  EXPECT_EQ(kStatusNik21, beacons[0]->status);
  EXPECT_EQ(kStatusNik31, beacons[1]->status);
  EXPECT_EQ(kStatusNik22, beacons[2]->status);

  // The next upload should automatically trigger.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(upload_isolation_info()));

  // Check that only the strings associated with the second NIK are present in
  // the report.
  EXPECT_EQ(upload_report().find(kStatusNik11), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik12), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik13), std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik21), std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik22), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik31), std::string::npos);
  // Complete upload.
  CallUploadCallback(successful_result);

  // There should still be 1 beacon queued.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(1u, beacons.size());
  EXPECT_TRUE(kIsolationInfo3.IsEqualForTesting(beacons[0]->isolation_info));
  EXPECT_EQ(kStatusNik31, beacons[0]->status);

  // The next upload should automatically trigger.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());
  EXPECT_TRUE(kIsolationInfo3.IsEqualForTesting(upload_isolation_info()));

  // Check that only the strings associated with the third NIK are present in
  // the report.
  EXPECT_EQ(upload_report().find(kStatusNik11), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik12), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik13), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik21), std::string::npos);
  EXPECT_EQ(upload_report().find(kStatusNik22), std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik31), std::string::npos);
  // Complete upload.
  CallUploadCallback(successful_result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Make sure that kMaxUploadDepthToSchedule is respected when requests have
// IsolationInfos with different NetworkIsolationKeys.
TEST_F(DomainReliabilityContextTest, ReportUploadDepthNetworkIsolationKeys) {
  const net::IsolationInfo kIsolationInfo1 =
      net::IsolationInfo::CreateTransient();
  const net::IsolationInfo kIsolationInfo2 =
      net::IsolationInfo::CreateTransient();

  InitContext(MakeTestConfig());

  const char kStatusNik1ExceedsMaxDepth[] = "nik1.exceeds_max_depth";
  const char kStatusNik2ExceedsMaxDepth[] = "nik2.exceeds_max_depth";
  const char kStatusNik2MaxDepth[] = "nik2.max_depth";

  // Add a beacon with kIsolationInfo1 and a depth that exceeds the max depth to
  // trigger an upload. No upload should be queued.
  std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeaconWithIsolationInfo(
      &time_, kStatusNik1ExceedsMaxDepth, kIsolationInfo1);
  beacon->upload_depth =
      DomainReliabilityContext::kMaxUploadDepthToSchedule + 1;
  context_->OnBeacon(std::move(beacon));
  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());

  // Add a beacon with kIsolationInfo2 and a depth that exceeds the max depth to
  // trigger an upload. No upload should be queued.
  beacon = MakeBeaconWithIsolationInfo(&time_, kStatusNik2ExceedsMaxDepth,
                                       kIsolationInfo2);
  beacon->upload_depth =
      DomainReliabilityContext::kMaxUploadDepthToSchedule + 1;
  context_->OnBeacon(std::move(beacon));
  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());

  // Add a beacon with kIsolationInfo2 and a depth that equals the max depth to
  // trigger an upload. An upload should be queued.
  beacon =
      MakeBeaconWithIsolationInfo(&time_, kStatusNik2MaxDepth, kIsolationInfo2);
  beacon->upload_depth = DomainReliabilityContext::kMaxUploadDepthToSchedule;
  context_->OnBeacon(std::move(beacon));
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());

  // All the beacons should still be queued.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(3u, beacons.size());
  EXPECT_TRUE(kIsolationInfo1.IsEqualForTesting(beacons[0]->isolation_info));
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(beacons[1]->isolation_info));
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(beacons[2]->isolation_info));
  EXPECT_EQ(kStatusNik1ExceedsMaxDepth, beacons[0]->status);
  EXPECT_EQ(kStatusNik2ExceedsMaxDepth, beacons[1]->status);
  EXPECT_EQ(kStatusNik2MaxDepth, beacons[2]->status);
  EXPECT_EQ(DomainReliabilityContext::kMaxUploadDepthToSchedule + 1,
            beacons[0]->upload_depth);
  EXPECT_EQ(DomainReliabilityContext::kMaxUploadDepthToSchedule + 1,
            beacons[1]->upload_depth);
  EXPECT_EQ(DomainReliabilityContext::kMaxUploadDepthToSchedule,
            beacons[2]->upload_depth);

  // Start the upload.
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(DomainReliabilityContext::kMaxUploadDepthToSchedule + 1,
            upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());
  EXPECT_TRUE(kIsolationInfo2.IsEqualForTesting(upload_isolation_info()));

  // Check that only the strings associated with the second NIK are present in
  // the report.
  EXPECT_EQ(upload_report().find(kStatusNik1ExceedsMaxDepth),
            std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik2ExceedsMaxDepth),
            std::string::npos);
  EXPECT_NE(upload_report().find(kStatusNik2MaxDepth), std::string::npos);

  // Complete upload.
  DomainReliabilityUploader::UploadResult successful_result;
  successful_result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(successful_result);

  // There should still be 1 beacon queued.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(1u, beacons.size());
  EXPECT_TRUE(kIsolationInfo1.IsEqualForTesting(beacons[0]->isolation_info));
  EXPECT_EQ(kStatusNik1ExceedsMaxDepth, beacons[0]->status);
  EXPECT_EQ(DomainReliabilityContext::kMaxUploadDepthToSchedule + 1,
            beacons[0]->upload_depth);

  // No upload should be queued, since the depth is too high.
  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());
}

TEST_F(DomainReliabilityContextTest, UploadForbidden) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(
      MakeCustomizedBeacon(&time_, "tcp.connection_reset", "", true));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(false);
  EXPECT_FALSE(upload_pending());
}

TEST_F(DomainReliabilityContextTest, NetworkChanged) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  // Simulate a network change after the request but before the upload.
  last_network_change_time_ = time_.NowTicks();
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  base::Value::Dict value = base::test::ParseJsonDict(upload_report());
  const Value::Dict* entry = GetEntryFromReport(value, 0);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(HasBooleanValue(*entry, "network_changed", true));

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Always expecting granular QUIC errors if status is quic.protocol error.
TEST_F(DomainReliabilityContextTest,
       ReportUploadWithQuicProtocolErrorAndQuicError) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(MakeCustomizedBeacon(&time_, "quic.protocol",
                                          "quic.invalid.stream_data", true));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  base::Value::Dict value = base::test::ParseJsonDict(upload_report());
  const Value::Dict* entry = GetEntryFromReport(value, 0);
  ASSERT_TRUE(entry);

  EXPECT_TRUE(HasBooleanValue(*entry, "quic_broken", true));
  EXPECT_TRUE(HasBooleanValue(*entry, "quic_port_migration_detected", true));
  EXPECT_TRUE(HasStringValue(*entry, "status", "quic.protocol"));
  EXPECT_TRUE(HasStringValue(*entry, "quic_error", "quic.invalid.stream_data"));

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

// If status is not quic.protocol, expect no granular QUIC error to be reported.
TEST_F(DomainReliabilityContextTest,
       ReportUploadWithNonQuicProtocolErrorAndNoQuicError) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  base::Value::Dict value = base::test::ParseJsonDict(upload_report());
  const Value::Dict* entry = GetEntryFromReport(value, 0);
  ASSERT_TRUE(entry);

  EXPECT_TRUE(HasStringValue(*entry, "status", "tcp.connection_reset"));
  EXPECT_FALSE(HasStringValue(*entry, "quic_error", ""));

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Edge cases that a non-QUIC protocol error with granular QUIC error reported,
// probably indicating state machine in http_network_transaction is working
// in a different way.
TEST_F(DomainReliabilityContextTest,
       ReportUploadWithNonQuicProtocolErrorAndQuicError) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(MakeCustomizedBeacon(&time_, "tcp.connection_reset",
                                          "quic.invalid.stream_data", false));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  base::Value::Dict value = base::test::ParseJsonDict(upload_report());
  const Value::Dict* entry = GetEntryFromReport(value, 0);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(HasBooleanValue(*entry, "quic_broken", true));
  EXPECT_TRUE(HasStringValue(*entry, "status", "tcp.connection_reset"));
  EXPECT_TRUE(HasStringValue(*entry, "quic_error", "quic.invalid.stream_data"));

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

TEST_F(DomainReliabilityContextTest, ZeroSampleRate) {
  std::unique_ptr<DomainReliabilityConfig> config(MakeTestConfig());
  config->failure_sample_rate = 0.0;
  InitContext(std::move(config));

  BeaconVector beacons;
  for (int i = 0; i < 100; i++) {
    context_->OnBeacon(MakeBeacon(&time_));
    EXPECT_TRUE(CheckNoBeacons());
  }
}

TEST_F(DomainReliabilityContextTest, FractionalSampleRate) {
  std::unique_ptr<DomainReliabilityConfig> config(MakeTestConfig());
  config->failure_sample_rate = 0.5;
  InitContext(std::move(config));

  BeaconVector beacons;
  do {
    context_->OnBeacon(MakeBeacon(&time_));
    context_->GetQueuedBeaconsForTesting(&beacons);
  } while (beacons.empty());
  EXPECT_EQ(1u, beacons.size());

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(0, upload_max_depth());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  base::Value::Dict value = base::test::ParseJsonDict(upload_report());
  const Value::Dict* entry = GetEntryFromReport(value, 0);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(HasDoubleValue(*entry, "sample_rate", 0.5));

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

TEST_F(DomainReliabilityContextTest, FailureSampleOnly) {
  std::unique_ptr<DomainReliabilityConfig> config(MakeTestConfig());
  config->success_sample_rate = 0.0;
  config->failure_sample_rate = 1.0;
  InitContext(std::move(config));

  BeaconVector beacons;

  context_->OnBeacon(MakeBeacon(&time_));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  std::unique_ptr<DomainReliabilityBeacon> beacon(MakeBeacon(&time_));
  beacon->status = "ok";
  beacon->chrome_error = net::OK;
  context_->OnBeacon(std::move(beacon));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
}

TEST_F(DomainReliabilityContextTest, SuccessSampleOnly) {
  std::unique_ptr<DomainReliabilityConfig> config(MakeTestConfig());
  config->success_sample_rate = 1.0;
  config->failure_sample_rate = 0.0;
  InitContext(std::move(config));

  BeaconVector beacons;

  context_->OnBeacon(MakeBeacon(&time_));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(0u, beacons.size());

  std::unique_ptr<DomainReliabilityBeacon> beacon(MakeBeacon(&time_));
  beacon->status = "ok";
  beacon->chrome_error = net::OK;
  context_->OnBeacon(std::move(beacon));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
}

TEST_F(DomainReliabilityContextTest, SampleAllBeacons) {
  std::unique_ptr<DomainReliabilityConfig> config(MakeTestConfig());
  config->success_sample_rate = 1.0;
  config->failure_sample_rate = 1.0;
  InitContext(std::move(config));

  BeaconVector beacons;

  context_->OnBeacon(MakeBeacon(&time_));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  std::unique_ptr<DomainReliabilityBeacon> beacon(MakeBeacon(&time_));
  beacon->status = "ok";
  beacon->chrome_error = net::OK;
  context_->OnBeacon(std::move(beacon));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(2u, beacons.size());
}

TEST_F(DomainReliabilityContextTest, SampleNoBeacons) {
  std::unique_ptr<DomainReliabilityConfig> config(MakeTestConfig());
  config->success_sample_rate = 0.0;
  config->failure_sample_rate = 0.0;
  InitContext(std::move(config));

  BeaconVector beacons;

  context_->OnBeacon(MakeBeacon(&time_));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(0u, beacons.size());

  std::unique_ptr<DomainReliabilityBeacon> beacon(MakeBeacon(&time_));
  beacon->status = "ok";
  beacon->chrome_error = net::OK;
  context_->OnBeacon(std::move(beacon));
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(0u, beacons.size());
}

TEST_F(DomainReliabilityContextTest, ExpiredBeaconDoesNotUpload) {
  base::HistogramTester histograms;
  InitContext(MakeTestConfig());
  std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeacon(&time_);
  time_.Advance(base::Hours(2));
  context_->OnBeacon(std::move(beacon));

  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_TRUE(beacons.empty());

  histograms.ExpectBucketCount(kBeaconOutcomeHistogram,
                               DomainReliabilityBeacon::Outcome::kExpired, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);

  ShutDownContext();
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);
}

TEST_F(DomainReliabilityContextTest, EvictOldestBeacon) {
  base::HistogramTester histograms;
  InitContext(MakeTestConfig());

  std::unique_ptr<DomainReliabilityBeacon> oldest_beacon = MakeBeacon(&time_);
  const DomainReliabilityBeacon* oldest_beacon_ptr = oldest_beacon.get();
  time_.Advance(base::Seconds(1));
  context_->OnBeacon(std::move(oldest_beacon));

  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeacon(&time_);
    time_.Advance(base::Seconds(1));
    context_->OnBeacon(std::move(beacon));
  }

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());

  for (const DomainReliabilityBeacon* beacon : beacons) {
    EXPECT_NE(oldest_beacon_ptr, beacon);
  }

  histograms.ExpectBucketCount(kBeaconOutcomeHistogram,
                               DomainReliabilityBeacon::Outcome::kEvicted, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);

  ShutDownContext();
  histograms.ExpectBucketCount(
      kBeaconOutcomeHistogram,
      DomainReliabilityBeacon::Outcome::kContextShutDown,
      DomainReliabilityContext::kMaxQueuedBeacons);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram,
                              1 + DomainReliabilityContext::kMaxQueuedBeacons);
}

// Test eviction when there's no active upload.
TEST_F(DomainReliabilityContextTest, Eviction) {
  InitContext(MakeTestConfig());

  // Add |DomainReliabilityContext::kMaxQueuedBeacons| beacons.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    context_->OnBeacon(
        MakeCustomizedBeacon(&time_, StatusFromInt(i), "" /* quic_error */,
                             false /* quic_port_migration_detected */));
  }

  // No beacons should have been evicted.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i));
  }

  // Add one more beacon.
  context_->OnBeacon(MakeCustomizedBeacon(
      &time_, StatusFromInt(DomainReliabilityContext::kMaxQueuedBeacons),
      "" /* quic_error */, false /* quic_port_migration_detected */));

  // The first beacon should have been evicted.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i + 1));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // All beacons but the first should be in the report.
  EXPECT_EQ(upload_report().find(StatusFromInt(0)), std::string::npos);
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_NE(upload_report().find(StatusFromInt(i + 1)), std::string::npos);
  }

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Test eviction when there's an upload that eventually succeeds.
TEST_F(DomainReliabilityContextTest, EvictionDuringSuccessfulUpload) {
  InitContext(MakeTestConfig());

  // Add |DomainReliabilityContext::kMaxQueuedBeacons| beacons.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    context_->OnBeacon(
        MakeCustomizedBeacon(&time_, StatusFromInt(i), "" /* quic_error */,
                             false /* quic_port_migration_detected */));
  }

  // No beacons should have been evicted.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // All beacons should be in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
  }

  // Add one more beacon.
  context_->OnBeacon(MakeCustomizedBeacon(
      &time_, StatusFromInt(DomainReliabilityContext::kMaxQueuedBeacons),
      "" /* quic_error */, false /* quic_port_migration_detected */));

  // The first beacon should have been evicted.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i + 1));
  }

  // The upload completes.
  DomainReliabilityUploader::UploadResult successful_result;
  successful_result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(successful_result);

  // The last beacon should still be queued.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(1u, beacons.size());
  EXPECT_EQ(beacons[0]->status,
            StatusFromInt(DomainReliabilityContext::kMaxQueuedBeacons));

  // Another upload should have still been queued, for the new report. Wait for
  // it to start uploading.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // Only the last beacon should be in the report.
  EXPECT_NE(upload_report().find(
                StatusFromInt(DomainReliabilityContext::kMaxQueuedBeacons)),
            std::string::npos);
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(upload_report().find(StatusFromInt(i)), std::string::npos);
  }

  // The upload completes.
  CallUploadCallback(successful_result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Test eviction when there's an upload that eventually fails.
TEST_F(DomainReliabilityContextTest, EvictionDuringUnsuccessfulUpload) {
  InitContext(MakeTestConfig());

  // Add |DomainReliabilityContext::kMaxQueuedBeacons| beacons.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    context_->OnBeacon(
        MakeCustomizedBeacon(&time_, StatusFromInt(i), "" /* quic_error */,
                             false /* quic_port_migration_detected */));
  }

  // No beacons should have been evicted.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // All beacons should be in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
  }

  // Add one more beacon.
  context_->OnBeacon(MakeCustomizedBeacon(
      &time_, StatusFromInt(DomainReliabilityContext::kMaxQueuedBeacons),
      "" /* quic_error */, false /* quic_port_migration_detected */));

  // The first beacon should have been evicted.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i + 1));
  }

  // The upload fails.
  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::FAILURE;
  CallUploadCallback(result);

  // All beacons but the first should still be queued.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i + 1));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // All beacons but the first should be in the report.
  EXPECT_EQ(upload_report().find(StatusFromInt(0)), std::string::npos);
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_NE(upload_report().find(StatusFromInt(i + 1)), std::string::npos);
  }

  // The upload completes successfully.
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Test eviction of all initially pending reports when there's an upload that
// eventually succeeds.
TEST_F(DomainReliabilityContextTest, EvictAllDuringSuccessfulUpload) {
  InitContext(MakeTestConfig());

  // Add |DomainReliabilityContext::kMaxQueuedBeacons| beacons.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    context_->OnBeacon(
        MakeCustomizedBeacon(&time_, StatusFromInt(i), "" /* quic_error */,
                             false /* quic_port_migration_detected */));
  }

  // No beacons should have been evicted.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // All beacons should be in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
  }

  // Evict all beacons, twice. It's important to add a beacon after all beacons
  // from the original report have already been deleted, to make sure that
  // eviction works correctly once |uploading_beacons_size_| reaches 0.
  for (size_t i = 0; i < 2 * DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    context_->OnBeacon(MakeCustomizedBeacon(
        &time_, StatusFromInt(i + DomainReliabilityContext::kMaxQueuedBeacons),
        "" /* quic_error */, false /* quic_port_migration_detected */));
  }

  // All the original beacons should have been evicted, as should the first
  // |DomainReliabilityContext::kMaxQueuedBeacons| beacons from the above loop.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(
        beacons[i]->status,
        StatusFromInt(i + 2 * DomainReliabilityContext::kMaxQueuedBeacons));
  }

  // The upload succeeds, but no beacons should be removed, since all the
  // original beacons have already been evicted.
  DomainReliabilityUploader::UploadResult successful_result;
  successful_result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(successful_result);

  // The same beacons as before should be queued.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(
        beacons[i]->status,
        StatusFromInt(i + 2 * DomainReliabilityContext::kMaxQueuedBeacons));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  // Check the expected beacons are in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons * 3; ++i) {
    if (i < DomainReliabilityContext::kMaxQueuedBeacons * 2) {
      EXPECT_EQ(upload_report().find(StatusFromInt(i)), std::string::npos);
    } else {
      EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
    }
  }

  // The upload completes successfully.
  CallUploadCallback(successful_result);

  EXPECT_TRUE(CheckNoBeacons());
}

// Make sure that evictions account for when there are IsolationInfos with
// different NetworkIsolationKeys in use.
TEST_F(DomainReliabilityContextTest,
       EvictionDuringSuccessfulUploadNetworkIsolationKey) {
  ASSERT_EQ(0u, DomainReliabilityContext::kMaxQueuedBeacons % 2)
      << "DomainReliabilityContext::kMaxQueuedBeacons must be even.";

  InitContext(MakeTestConfig());

  net::IsolationInfo isolation_infos[] = {
      net::IsolationInfo::CreateTransient(),
      net::IsolationInfo::CreateTransient(),
  };

  // Add `DomainReliabilityContext::kMaxQueuedBeacons` beacons, using a
  // different IsolationInfo for every other beacon.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    context_->OnBeacon(MakeBeaconWithIsolationInfo(&time_, StatusFromInt(i),
                                                   isolation_infos[i % 2]));
  }

  // No beacons should have been evicted.
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i));
    EXPECT_TRUE(
        beacons[i]->isolation_info.IsEqualForTesting(isolation_infos[i % 2]));
  }

  // Wait for the report to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_TRUE(isolation_infos[0].IsEqualForTesting(upload_isolation_info()));
  // All even-numbered beacons should be in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    if (i % 2 == 0) {
      EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
    } else {
      EXPECT_EQ(upload_report().find(StatusFromInt(i)), std::string::npos);
    }
  }

  // Add two more beacons, using the same pattern as before
  for (size_t i = 0; i < 2; ++i) {
    context_->OnBeacon(MakeBeaconWithIsolationInfo(
        &time_, StatusFromInt(i + DomainReliabilityContext::kMaxQueuedBeacons),
        isolation_infos[i % 2]));
  }

  // Only the first two beacons should have been evicted.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons, beacons.size());
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    EXPECT_EQ(beacons[i]->status, StatusFromInt(i + 2));
    EXPECT_TRUE(
        beacons[i]->isolation_info.IsEqualForTesting(isolation_infos[i % 2]));
  }

  // The upload succeeds.  Every beacon using the first IsolationInfo, except
  // the second to last, should have been evicted.
  DomainReliabilityUploader::UploadResult successful_result;
  successful_result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(successful_result);

  // Check remaining beacons.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(DomainReliabilityContext::kMaxQueuedBeacons / 2 + 1,
            beacons.size());
  int beacon_index = 0;
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons; ++i) {
    if (i % 2 == 0 && i < DomainReliabilityContext::kMaxQueuedBeacons - 2)
      continue;
    EXPECT_EQ(beacons[beacon_index]->status, StatusFromInt(i + 2));
    EXPECT_TRUE(beacons[beacon_index]->isolation_info.IsEqualForTesting(
        isolation_infos[i % 2]));
    beacon_index++;
  }

  // Another report should be queued.  Wait for it to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_TRUE(isolation_infos[1].IsEqualForTesting(upload_isolation_info()));
  // Check the expected beacons are in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons + 2; ++i) {
    if (i % 2 == 0 || i < 2) {
      EXPECT_EQ(upload_report().find(StatusFromInt(i)), std::string::npos);
    } else {
      EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
    }
  }

  // The upload completes successfully.
  CallUploadCallback(successful_result);

  // Check remaining beacons. There should only be one left.
  context_->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(1u, beacons.size());
  EXPECT_EQ(beacons[0]->status,
            StatusFromInt(DomainReliabilityContext::kMaxQueuedBeacons));
  EXPECT_TRUE(beacons[0]->isolation_info.IsEqualForTesting(isolation_infos[0]));

  // Another report should be queued.  Wait for it to start being uploaded.
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_allowed_callback_pending());
  CallUploadAllowedResultCallback(true);
  EXPECT_TRUE(upload_pending());
  EXPECT_TRUE(isolation_infos[0].IsEqualForTesting(upload_isolation_info()));
  // Check the expected beacons are in the report.
  for (size_t i = 0; i < DomainReliabilityContext::kMaxQueuedBeacons + 2; ++i) {
    if (i == DomainReliabilityContext::kMaxQueuedBeacons) {
      EXPECT_NE(upload_report().find(StatusFromInt(i)), std::string::npos);
    } else {
      EXPECT_EQ(upload_report().find(StatusFromInt(i)), std::string::npos);
    }
  }

  // The upload completes successfully.
  CallUploadCallback(successful_result);

  EXPECT_TRUE(CheckNoBeacons());
}

// TODO(juliatuttle): Add beacon_unittest.cc to test serialization.

}  // namespace
}  // namespace domain_reliability

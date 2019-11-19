// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/test_util.h"
#include "components/domain_reliability/uploader.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

using base::DictionaryValue;
using base::ListValue;
using base::Value;

typedef std::vector<const DomainReliabilityBeacon*> BeaconVector;

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
  beacon->elapsed = base::TimeDelta::FromMilliseconds(250);
  beacon->start_time = time->NowTicks() - beacon->elapsed;
  beacon->upload_depth = 0;
  beacon->sample_rate = 1.0;
  return beacon;
}

std::unique_ptr<DomainReliabilityBeacon> MakeBeacon(MockableTime* time) {
  return MakeCustomizedBeacon(time, "tcp.connection_reset", "", false);
}

template <typename ValueType,
          bool (DictionaryValue::*GetValueType)(base::StringPiece, ValueType*)
              const>
struct HasValue {
  bool operator()(const DictionaryValue& dict,
                  const std::string& key,
                  ValueType expected_value) {
    ValueType actual_value;
    bool got_value = (dict.*GetValueType)(key, &actual_value);
    if (got_value)
      EXPECT_EQ(expected_value, actual_value);
    return got_value && (expected_value == actual_value);
  }
};

HasValue<bool, &DictionaryValue::GetBoolean> HasBooleanValue;
HasValue<double, &DictionaryValue::GetDouble> HasDoubleValue;
HasValue<int, &DictionaryValue::GetInteger> HasIntegerValue;
HasValue<std::string, &DictionaryValue::GetString> HasStringValue;

bool GetEntryFromReport(const Value* report,
                        size_t index,
                        const DictionaryValue** entry_out) {
  const DictionaryValue* report_dict;
  const ListValue* entries;

  return report &&
         report->GetAsDictionary(&report_dict) &&
         report_dict->GetList("entries", &entries) &&
         entries->GetDictionary(index, entry_out);
}

class DomainReliabilityContextTest : public testing::Test {
 protected:
  DomainReliabilityContextTest()
      : last_network_change_time_(time_.NowTicks()),
        dispatcher_(&time_),
        params_(MakeTestSchedulerParams()),
        uploader_(base::Bind(&DomainReliabilityContextTest::OnUploadRequest,
                             base::Unretained(this))),
        upload_reporter_string_("test-reporter"),
        upload_allowed_callback_(
            base::Bind(&DomainReliabilityContextTest::UploadAllowedCallback,
                       base::Unretained(this))),
        upload_pending_(false) {
    // Make sure that the last network change does not overlap requests
    // made in test cases, which start 250ms in the past (see |MakeBeacon|).
    last_network_change_time_ = time_.NowTicks();
    time_.Advance(base::TimeDelta::FromSeconds(1));
  }

  void InitContext(std::unique_ptr<const DomainReliabilityConfig> config) {
    context_.reset(new DomainReliabilityContext(
        &time_, params_, upload_reporter_string_, &last_network_change_time_,
        upload_allowed_callback_, &dispatcher_, &uploader_, std::move(config)));
  }

  base::TimeDelta min_delay() const { return params_.minimum_upload_delay; }
  base::TimeDelta max_delay() const { return params_.maximum_upload_delay; }
  base::TimeDelta retry_interval() const {
    return params_.upload_retry_interval;
  }
  base::TimeDelta zero_delta() const {
    return base::TimeDelta::FromMicroseconds(0);
  }

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

  void CallUploadCallback(DomainReliabilityUploader::UploadResult result) {
    ASSERT_TRUE(upload_pending_);
    upload_callback_.Run(result);
    upload_pending_ = false;
  }

  bool CheckNoBeacons() {
    BeaconVector beacons;
    context_->GetQueuedBeaconsForTesting(&beacons);
    return beacons.empty();
  }

  const GURL& upload_allowed_origin() { return upload_allowed_origin_; }

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
  void OnUploadRequest(
      const std::string& report_json,
      int max_upload_depth,
      const GURL& upload_url,
      const DomainReliabilityUploader::UploadCallback& callback) {
    ASSERT_FALSE(upload_pending_);
    upload_report_ = report_json;
    upload_max_depth_ = max_upload_depth;
    upload_url_ = upload_url;
    upload_callback_ = callback;
    upload_pending_ = true;
  }

  void UploadAllowedCallback(const GURL& origin,
                             base::OnceCallback<void(bool)> callback) {
    upload_allowed_origin_ = origin;
    upload_allowed_result_callback_ = std::move(callback);
  }

  bool upload_pending_;
  std::string upload_report_;
  int upload_max_depth_;
  GURL upload_url_;
  DomainReliabilityUploader::UploadCallback upload_callback_;

  GURL upload_allowed_origin_;
  base::OnceCallback<void(bool)> upload_allowed_result_callback_;
};

TEST_F(DomainReliabilityContextTest, Create) {
  InitContext(MakeTestConfig());
  EXPECT_TRUE(CheckNoBeacons());
}

TEST_F(DomainReliabilityContextTest, Report) {
  InitContext(MakeTestConfig());
  context_->OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
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

  EXPECT_TRUE(CheckNoBeacons());
}

TEST_F(DomainReliabilityContextTest, ReportUpload) {
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

  std::unique_ptr<Value> value =
      base::JSONReader::ReadDeprecated(upload_report());
  const DictionaryValue* entry;
  ASSERT_TRUE(GetEntryFromReport(value.get(), 0, &entry));
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

  EXPECT_TRUE(CheckNoBeacons());
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

  std::unique_ptr<Value> value =
      base::JSONReader::ReadDeprecated(upload_report());
  const DictionaryValue* entry;
  ASSERT_TRUE(GetEntryFromReport(value.get(), 0, &entry));
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

  std::unique_ptr<Value> value =
      base::JSONReader::ReadDeprecated(upload_report());
  const DictionaryValue* entry;
  ASSERT_TRUE(GetEntryFromReport(value.get(), 0, &entry));

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

  std::unique_ptr<Value> value =
      base::JSONReader::ReadDeprecated(upload_report());
  const DictionaryValue* entry;
  ASSERT_TRUE(GetEntryFromReport(value.get(), 0, &entry));

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

  std::unique_ptr<Value> value =
      base::JSONReader::ReadDeprecated(upload_report());
  const DictionaryValue* entry;
  ASSERT_TRUE(GetEntryFromReport(value.get(), 0, &entry));
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

  std::unique_ptr<Value> value =
      base::JSONReader::ReadDeprecated(upload_report());
  const DictionaryValue* entry;
  ASSERT_TRUE(GetEntryFromReport(value.get(), 0, &entry));
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
  InitContext(MakeTestConfig());
  std::unique_ptr<DomainReliabilityBeacon> beacon = MakeBeacon(&time_);
  time_.Advance(base::TimeDelta::FromHours(2));
  context_->OnBeacon(std::move(beacon));

  time_.Advance(max_delay());
  EXPECT_FALSE(upload_allowed_callback_pending());
  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_TRUE(beacons.empty());
}

// TODO(juliatuttle): Add beacon_unittest.cc to test serialization.

}  // namespace
}  // namespace domain_reliability

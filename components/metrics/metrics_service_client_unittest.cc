// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test_metrics_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

class MetricsServiceClientTest : public testing::Test {
 public:
  MetricsServiceClientTest() {}
  ~MetricsServiceClientTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceClientTest);
};

}  // namespace

TEST_F(MetricsServiceClientTest, TestUploadIntervalDefaultsToStandard) {
  TestMetricsServiceClient client;

  ASSERT_EQ(client.GetStandardUploadInterval(), client.GetUploadInterval());
}

TEST_F(MetricsServiceClientTest, TestModifyMetricsUploadInterval) {
  TestMetricsServiceClient client;

  // Flip it a few times to make sure we really can modify it. Values are
  // arbitrary (but positive, because the upload interval should be).
  int specified_upload_sec = 800;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMetricsUploadIntervalSec,
      base::NumberToString(specified_upload_sec));
  ASSERT_EQ(base::TimeDelta::FromSeconds(specified_upload_sec),
            client.GetUploadInterval());

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kMetricsUploadIntervalSec);

  specified_upload_sec = 30;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMetricsUploadIntervalSec,
      base::NumberToString(specified_upload_sec));
  ASSERT_EQ(base::TimeDelta::FromSeconds(specified_upload_sec),
            client.GetUploadInterval());
}

TEST_F(MetricsServiceClientTest, TestUploadIntervalLimitedForDos) {
  TestMetricsServiceClient client;

  // If we set the upload interval too small, it should be limited to prevent
  // the possibility of DOS'ing the backend. This should be a safe guess for a
  // value strictly smaller than the DOS limit.
  int too_short_upload_sec = 2;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMetricsUploadIntervalSec,
      base::NumberToString(too_short_upload_sec));
  // Upload interval should be the DOS rate limit.
  ASSERT_EQ(base::TimeDelta::FromSeconds(20), client.GetUploadInterval());
}

}  // namespace metrics

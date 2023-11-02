// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/log_decoder.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

TEST(LogDecoderTest, DecodeLogDataToProto) {
  ChromeUserMetricsExtension uma_log1;
  uma_log1.mutable_system_profile()->set_application_locale("fr");

  std::string log_data1;
  ASSERT_TRUE(uma_log1.SerializeToString(&log_data1));
  std::string compressed_log_data;
  ASSERT_TRUE(compression::GzipCompress(log_data1, &compressed_log_data));

  ChromeUserMetricsExtension uma_log2;
  EXPECT_TRUE(DecodeLogDataToProto(compressed_log_data, &uma_log2));

  std::string log_data2;
  uma_log2.SerializeToString(&log_data2);
  EXPECT_EQ(log_data1, log_data2);
}

}  // namespace metrics

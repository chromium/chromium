// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/uninstall_metrics.h"

#include <memory>
#include <string>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string16.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

TEST(UninstallMetricsTest, TestExtractUninstallMetrics) {
  // A make-believe JSON preferences file.
  std::string pref_string(
      "{ \n"
      "  \"foo\": \"bar\",\n"
      "  \"uninstall_metrics\": { \n"
      "    \"last_launch_time_sec\": \"1235341118\","
      "    \"last_observed_running_time_sec\": \"1235341183\","
      "    \"launch_count\": \"11\","
      "    \"page_load_count\": \"68\","
      "    \"uptime_sec\": \"809\","
      "    \"installation_date2\": \"1235341141\"\n"
      "  },\n"
      "  \"blah\": {\n"
      "    \"this_sentence_is_true\": false\n"
      "  },\n"
      "  \"user_experience_metrics\": { \n"
      "    \"client_id_timestamp\": \"1234567890\","
      "    \"reporting_enabled\": true\n"
      "  }\n"
      "} \n");

  // The URL string we expect to be generated from said make-believe file.
  base::string16 expected_url_string(
      L"&installation_date2=1235341141"
      L"&last_launch_time_sec=1235341118"
      L"&last_observed_running_time_sec=1235341183"
      L"&launch_count=11&page_load_count=68"
      L"&uptime_sec=809");

  JSONStringValueDeserializer json_deserializer(pref_string);
  std::string error_message;

  std::unique_ptr<base::Value> root =
      json_deserializer.Deserialize(nullptr, &error_message);
  ASSERT_TRUE(root.get());
  base::string16 uninstall_metrics_string;

  EXPECT_TRUE(ExtractUninstallMetrics(*root, &uninstall_metrics_string));
  EXPECT_EQ(expected_url_string, uninstall_metrics_string);
}

}  // namespace installer

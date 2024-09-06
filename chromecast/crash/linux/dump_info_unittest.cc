// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/crash/linux/crash_testing_utils.h"
#include "chromecast/crash/linux/dump_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(DumpInfoTest, EmptyStringIsNotValid) {
  std::unique_ptr<DumpInfo> dump_info(CreateDumpInfo(""));
  ASSERT_FALSE(dump_info->valid());
}

TEST(DumpInfoTest, TooFewFieldsIsNotValid) {
  std::unique_ptr<DumpInfo> dump_info(
      CreateDumpInfo("{"
                     "\"name\": \"name\","
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"dump_string\""
                     "}"));
  ASSERT_FALSE(dump_info->valid());
}

TEST(DumpInfoTest, BadTimeStringIsNotValid) {
  std::unique_ptr<DumpInfo> info(
      CreateDumpInfo("{"
                     "\"name\": \"name\","
                     "\"dump_time\" : \"What up\","
                     "\"dump\": \"dump_string\","
                     "\"uptime\": \"123456789\","
                     "\"logfile\": \"logfile.log\""
                     "}"));
  ASSERT_FALSE(info->valid());
}

TEST(DumpInfoTest, AllRequiredFieldsIsValid) {
  std::unique_ptr<DumpInfo> info(
      CreateDumpInfo("{"
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"dump_string\","
                     "\"uptime\": \"123456789\","
                     "\"logfile\": \"logfile.log\""
                     "}"));
  static constexpr base::Time::Exploded kTime = {.year = 2001,
                                                 .month = 11,
                                                 .day_of_month = 12,
                                                 .hour = 18,
                                                 .minute = 31,
                                                 .second = 1};
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kTime, &dump_time));

  ASSERT_TRUE(info->valid());
  ASSERT_EQ(dump_time, info->dump_time());
  ASSERT_EQ("dump_string", info->crashed_process_dump());
  ASSERT_EQ(123456789u, info->params().process_uptime);
  ASSERT_EQ("logfile.log", info->logfile());
}

TEST(DumpInfoTest, EmptyProcessNameIsValid) {
  std::unique_ptr<DumpInfo> dump_info(
      CreateDumpInfo("{"
                     "\"name\": \"\","
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"dump_string\","
                     "\"uptime\": \"123456789\","
                     "\"logfile\": \"logfile.log\""
                     "}"));
  ASSERT_TRUE(dump_info->valid());
}

TEST(DumpInfoTest, SomeRequiredFieldsEmptyIsValid) {
  std::unique_ptr<DumpInfo> info(
      CreateDumpInfo("{"
                     "\"name\": \"name\","
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"\","
                     "\"uptime\": \"\","
                     "\"logfile\": \"\""
                     "}"));
  static constexpr base::Time::Exploded kTime = {.year = 2001,
                                                 .month = 11,
                                                 .day_of_month = 12,
                                                 .hour = 18,
                                                 .minute = 31,
                                                 .second = 1};
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kTime, &dump_time));

  ASSERT_TRUE(info->valid());
  ASSERT_EQ(dump_time, info->dump_time());
  ASSERT_EQ("", info->crashed_process_dump());
  ASSERT_EQ(0u, info->params().process_uptime);
  ASSERT_EQ("", info->logfile());
}

TEST(DumpInfoTest, AllOptionalFieldsIsValid) {
  std::unique_ptr<DumpInfo> info(
      CreateDumpInfo("{"
                     "\"name\": \"name\","
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"dump_string\","
                     "\"uptime\": \"123456789\","
                     "\"logfile\": \"logfile.log\","
                     "\"attachments\": [\"file1.txt\", \"file2.img\"],"
                     "\"suffix\": \"suffix\","
                     "\"prev_app_name\": \"previous_app\","
                     "\"cur_app_name\": \"current_app\","
                     "\"last_app_name\": \"last_app\","
                     "\"release_version\": \"RELEASE\","
                     "\"build_number\": \"BUILD_NUMBER\","
                     "\"reason\": \"foo\","
                     "\"comments\": \"comments\","
                     "\"js_engine\": \"js_engine\","
                     "\"js_build_label\": \"js_build_label\","
                     "\"js_exception_category\": \"js_exception_category\","
                     "\"js_exception_details\": \"js_exception_details\","
                     "\"js_exception_signature\": \"js_exception_signature\""
                     "}"));
  static constexpr base::Time::Exploded kTime = {.year = 2001,
                                                 .month = 11,
                                                 .day_of_month = 12,
                                                 .hour = 18,
                                                 .minute = 31,
                                                 .second = 1};
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kTime, &dump_time));

  ASSERT_TRUE(info->valid());
  ASSERT_EQ(dump_time, info->dump_time());
  ASSERT_EQ("dump_string", info->crashed_process_dump());
  ASSERT_EQ(123456789u, info->params().process_uptime);
  ASSERT_EQ("logfile.log", info->logfile());

  auto attachments = info->attachments();
  ASSERT_EQ(2u, attachments.size());
  ASSERT_EQ("file1.txt", attachments[0]);
  ASSERT_EQ("file2.img", attachments[1]);
  ASSERT_EQ("suffix", info->params().suffix);
  ASSERT_EQ("previous_app", info->params().previous_app_name);
  ASSERT_EQ("current_app", info->params().current_app_name);
  ASSERT_EQ("last_app", info->params().last_app_name);
  ASSERT_EQ("foo", info->params().reason);

  ASSERT_EQ("comments", info->params().comments);
  ASSERT_EQ("js_engine", info->params().js_engine);
  ASSERT_EQ("js_build_label", info->params().js_build_label);
  ASSERT_EQ("js_exception_category", info->params().js_exception_category);
  ASSERT_EQ("js_exception_details", info->params().js_exception_details);
  ASSERT_EQ("js_exception_signature", info->params().js_exception_signature);
}

TEST(DumpInfoTest, SomeOptionalFieldsIsValid) {
  std::unique_ptr<DumpInfo> info(
      CreateDumpInfo("{"
                     "\"name\": \"name\","
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"dump_string\","
                     "\"uptime\": \"123456789\","
                     "\"logfile\": \"logfile.log\","
                     "\"suffix\": \"suffix\","
                     "\"prev_app_name\": \"previous_app\","
                     "\"comments\": \"my comments\","
                     "\"js_engine\": \"js_engine version\","
                     "\"js_build_label\": \"js_build_label debug\""
                     "}"));
  static constexpr base::Time::Exploded kTime = {.year = 2001,
                                                 .month = 11,
                                                 .day_of_month = 12,
                                                 .hour = 18,
                                                 .minute = 31,
                                                 .second = 1};
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(kTime, &dump_time));

  ASSERT_TRUE(info->valid());
  ASSERT_EQ(dump_time, info->dump_time());
  ASSERT_EQ("dump_string", info->crashed_process_dump());
  ASSERT_EQ(123456789u, info->params().process_uptime);
  ASSERT_EQ("logfile.log", info->logfile());

  ASSERT_EQ("suffix", info->params().suffix);
  ASSERT_EQ("previous_app", info->params().previous_app_name);

  ASSERT_EQ("my comments", info->params().comments);
  ASSERT_EQ("js_engine version", info->params().js_engine);
  ASSERT_EQ("js_build_label debug", info->params().js_build_label);
}

TEST(DumpInfoTest, ExtraFieldsIsNotValid) {
  std::unique_ptr<DumpInfo> info(
      CreateDumpInfo("{"
                     "\"name\": \"name\","
                     "\"dump_time\" : \"2001-11-12 18:31:01\","
                     "\"dump\": \"dump_string\","
                     "\"uptime\": \"123456789\","
                     "\"logfile\": \"logfile.log\","
                     "\"suffix\": \"suffix\","
                     "\"prev_app_name\": \"previous_app\","
                     "\"cur_app_name\": \"current_app\","
                     "\"last_app_name\": \"last_app\","
                     "\"release_version\": \"RELEASE\","
                     "\"build_number\": \"BUILD_NUMBER\","
                     "\"comments\": \"comments\","
                     "\"js_engine\": \"js_engine\","
                     "\"js_build_label\": \"js_build_label\","
                     "\"js_exception_category\": \"js_exception_category\","
                     "\"js_exception_details\": \"js_exception_details\","
                     "\"js_exception_signature\": \"js_exception_signature\","
                     "\"hello\": \"extra_field\""
                     "}"));
  ASSERT_FALSE(info->valid());
}

}  // namespace chromecast

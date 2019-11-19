// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

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
                     "\"dump_time\" : \"Mar 23 2014 01:23:45\","
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
  base::Time::Exploded ex = {0};
  ex.second = 1;
  ex.minute = 31;
  ex.hour = 18;
  ex.day_of_month = 12;
  ex.month = 11;
  ex.year = 2001;
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(ex, &dump_time));

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
  base::Time::Exploded ex = {0};
  ex.second = 1;
  ex.minute = 31;
  ex.hour = 18;
  ex.day_of_month = 12;
  ex.month = 11;
  ex.year = 2001;
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(ex, &dump_time));

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
                     "\"suffix\": \"suffix\","
                     "\"prev_app_name\": \"previous_app\","
                     "\"cur_app_name\": \"current_app\","
                     "\"last_app_name\": \"last_app\","
                     "\"release_version\": \"RELEASE\","
                     "\"build_number\": \"BUILD_NUMBER\","
                     "\"reason\": \"foo\""
                     "}"));
  base::Time::Exploded ex = {0};
  ex.second = 1;
  ex.minute = 31;
  ex.hour = 18;
  ex.day_of_month = 12;
  ex.month = 11;
  ex.year = 2001;
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(ex, &dump_time));

  ASSERT_TRUE(info->valid());
  ASSERT_EQ(dump_time, info->dump_time());
  ASSERT_EQ("dump_string", info->crashed_process_dump());
  ASSERT_EQ(123456789u, info->params().process_uptime);
  ASSERT_EQ("logfile.log", info->logfile());

  ASSERT_EQ("suffix", info->params().suffix);
  ASSERT_EQ("previous_app", info->params().previous_app_name);
  ASSERT_EQ("current_app", info->params().current_app_name);
  ASSERT_EQ("last_app", info->params().last_app_name);
  ASSERT_EQ("foo", info->params().reason);
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
                     "\"prev_app_name\": \"previous_app\""
                     "}"));
  base::Time::Exploded ex = {0};
  ex.second = 1;
  ex.minute = 31;
  ex.hour = 18;
  ex.day_of_month = 12;
  ex.month = 11;
  ex.year = 2001;
  base::Time dump_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(ex, &dump_time));

  ASSERT_TRUE(info->valid());
  ASSERT_EQ(dump_time, info->dump_time());
  ASSERT_EQ("dump_string", info->crashed_process_dump());
  ASSERT_EQ(123456789u, info->params().process_uptime);
  ASSERT_EQ("logfile.log", info->logfile());

  ASSERT_EQ("suffix", info->params().suffix);
  ASSERT_EQ("previous_app", info->params().previous_app_name);
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
                     "\"hello\": \"extra_field\""
                     "}"));
  ASSERT_FALSE(info->valid());
}

}  // namespace chromecast

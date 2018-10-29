// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Path to the test app used to locate sample app.
std::string s_test_app_path;

// Returns directory name with trailing separator extracted from the file path.
std::string DirName(const std::string& file_path) {
  size_t pos = file_path.find_last_of("\\/");
  if (std::string::npos == pos)
    return std::string();
  return file_path.substr(0, pos + 1);
}

// Runs |command_line| and returns string representation of its stdout.
std::string RunCommand(std::string command_line) {
  std::string result_out = "command_result.tmp";
  EXPECT_EQ(0, std::system((command_line + " >" + result_out).c_str()));
  std::stringstream result;
  result << std::ifstream(result_out).rdbuf();
  std::remove(result_out.c_str());
  return result.str();
}

// Test that cronet_sample runs and gets connection refused from localhost.
TEST(SampleTest, TestConnectionRefused) {
  // Expect "cronet_sample" app to be located in same directory as the test.
  std::string cronet_sample_path = DirName(s_test_app_path) + "cronet_sample";
  std::string url = "http://localhost:99999";
  std::string sample_out = RunCommand(cronet_sample_path + " " + url);

  // Expect cronet sample to run and fail with net::ERR_INVALID_URL.
  EXPECT_NE(std::string::npos, sample_out.find("net::ERR_INVALID_URL"));
}

}  // namespace

int main(int argc, char** argv) {
  s_test_app_path = argv[0];
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

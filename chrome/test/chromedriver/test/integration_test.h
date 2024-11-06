// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_TEST_INTEGRATION_TEST_H_
#define CHROME_TEST_CHROMEDRIVER_TEST_INTEGRATION_TEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/net/pipe_builder.h"
#include "chrome/test/chromedriver/net/test_http_server.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

testing::AssertionResult StatusOk(const Status& status);

class IntegrationTest : public ::testing::Test {
 protected:
  IntegrationTest();
  ~IntegrationTest() override;

  static void SetUpTestSuite();
  void SetUp() override;
  void TearDown() override;

  Status SetUpConnection();

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestHttpServer http_server_;
  base::ScopedTempDir user_data_dir_temp_dir_;
  base::Process process_;
  PipeBuilder pipe_builder_;
  std::unique_ptr<DevToolsClientImpl> browser_client_;
  BrowserInfo browser_info_;
  Session session_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_TEST_INTEGRATION_TEST_H_

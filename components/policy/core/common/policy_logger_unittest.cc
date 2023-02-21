// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"
#include "base/test/scoped_feature_list.h"
#include "components/policy/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Property;

namespace policy {

namespace {

void AddLogs(const std::string& message, PolicyLogger* policy_logger) {
  LOG_POLICY(INFO,POLICY_FETCHING) << "Element added " << message;
}

}  // namespace

TEST(PolicyLoggerTest, PolicyLoggingEnabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      policy::features::kPolicyLogsPageAndroid, true);

  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  size_t logs_size_before_adding = policy_logger->GetPolicyLogsSizeForTesting();
  AddLogs("when the feature is enabled.", policy_logger);
  // Check that logger is enabled by feature and that `GetAsList` returns an
  // updated list of logs.
  EXPECT_EQ(policy_logger->GetAsList().size(), logs_size_before_adding + 1);
  EXPECT_EQ(*(policy_logger->GetAsList()[logs_size_before_adding].FindStringKey(
                "message")),
            "Element added when the feature is enabled.");
}

TEST(PolicyLoggerTest, PolicyLoggingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      policy::features::kPolicyLogsPageAndroid, false);

  PolicyLogger* policy_logger = policy::PolicyLogger::GetInstance();

  size_t logs_size_before_adding = policy_logger->GetPolicyLogsSizeForTesting();
  AddLogs("when the feature is disabled.", policy_logger);
  EXPECT_EQ(policy_logger->GetPolicyLogsSizeForTesting(),
            logs_size_before_adding);
}

}  // namespace policy
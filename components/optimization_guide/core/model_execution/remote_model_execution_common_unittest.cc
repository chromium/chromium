// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/remote_model_execution_common.h"

#include <optional>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace {

using base::test::TestMessage;

TEST(RemoteModelExecutionCommonTest, GetModelExecutionServiceBaseURLDefault) {
  EXPECT_EQ(GetModelExecutionServiceBaseURL(),
            GURL(kOptimizationGuideServiceModelExecutionDefaultBaseURL));
}

TEST(RemoteModelExecutionCommonTest,
     GetModelExecutionServiceBaseURLOverriddenBySwitch) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kOptimizationGuideServiceModelExecutionURLSwitch, "https://foo.bar/");

  EXPECT_EQ(GetModelExecutionServiceBaseURL(), GURL("https://foo.bar/"));
}

TEST(RemoteModelExecutionCommonTest, GetModelExecutionServiceFullURLDefault) {
  EXPECT_EQ(GetModelExecutionServiceFullURL("v1:Execute"),
            GURL("https://chromemodelexecution-pa.googleapis.com/v1:Execute"));
}

TEST(RemoteModelExecutionCommonTest,
     GetModelExecutionServiceFullURLWithCustomBaseURL) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kOptimizationGuideServiceModelExecutionURLSwitch, "https://foo.bar/");

  EXPECT_EQ(GetModelExecutionServiceFullURL("v1:Execute"),
            GURL("https://foo.bar/v1:Execute"));
}

TEST(RemoteModelExecutionCommonTest,
     GetModelExecutionServiceFullURLWebSocketConvertsHttpsToWss) {
  EXPECT_EQ(GetModelExecutionServiceFullURLWebSocket("ws/StreamExecute"),
            GURL("wss://chromemodelexecution-pa.googleapis.com/"
                 "ws/StreamExecute"));
}

TEST(RemoteModelExecutionCommonTest,
     GetModelExecutionServiceFullURLWebSocketConvertsHttpToWs) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kOptimizationGuideServiceModelExecutionURLSwitch, "http://foo.bar/");

  EXPECT_EQ(GetModelExecutionServiceFullURLWebSocket("ws/StreamExecute"),
            GURL("ws://foo.bar/ws/StreamExecute"));
}

TEST(RemoteModelExecutionCommonTest,
     GetModelExecutionServiceFullURLWebSocketPreservesWsAndWssSchemes) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        kOptimizationGuideServiceModelExecutionURLSwitch, "wss://foo.bar/");

    EXPECT_EQ(GetModelExecutionServiceFullURLWebSocket("ws/StreamExecute"),
              GURL("wss://foo.bar/ws/StreamExecute"));
  }
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        kOptimizationGuideServiceModelExecutionURLSwitch, "ws://foo.bar/");

    EXPECT_EQ(GetModelExecutionServiceFullURLWebSocket("ws/StreamExecute"),
              GURL("ws://foo.bar/ws/StreamExecute"));
  }
}

TEST(RemoteModelExecutionCommonTest, CreateExecuteRequest) {
  TestMessage metadata;
  metadata.set_test("sample payload");

  proto::ExecuteRequest request =
      CreateExecuteRequest(ModelBasedCapabilityKey::kCompose, metadata);

  EXPECT_EQ(request.feature(),
            proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE);
  EXPECT_EQ(request.request_metadata().type_url(),
            "type.googleapis.com/base.test.TestMessage");
  std::optional<TestMessage> parsed_metadata =
      ParsedAnyMetadata<TestMessage>(request.request_metadata());
  ASSERT_TRUE(parsed_metadata.has_value());
  EXPECT_EQ(parsed_metadata->test(), "sample payload");
}

TEST(RemoteModelExecutionCommonTest, AppendHeadersIfNeededWithoutSwitch) {
  network::ResourceRequest request;
  AppendHeadersIfNeeded(request);

  EXPECT_FALSE(request.headers.HasHeader(
      kOptimizationGuideModelExecutionDebugLogsHeaderKey));
}

TEST(RemoteModelExecutionCommonTest, AppendHeadersIfNeededWithSwitch) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      kModelExecutionEnableRemoteDebugLoggingSwitch);

  network::ResourceRequest request;
  AppendHeadersIfNeeded(request);

  EXPECT_TRUE(request.headers.HasHeader(
      kOptimizationGuideModelExecutionDebugLogsHeaderKey));
}

}  // namespace
}  // namespace optimization_guide

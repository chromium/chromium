// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/frame_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FrameTracker, GetContextIdForFrame) {
  StubDevToolsClient client;
  FrameTracker tracker(&client);
  std::string context_id;
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsError());
  ASSERT_EQ("", context_id);

  const char context[] =
      "{\"uniqueId\":\"100\",\"auxData\":{\"frameId\":\"f\",\"isDefault\":true}"
      "}";
  base::Value::Dict params;
  params.Set("context", base::test::ParseJson(context));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Runtime.executionContextCreated", params)
                .code());
  ASSERT_EQ(kNoSuchExecutionContext,
            tracker.GetContextIdForFrame("foo", &context_id).code());
  ASSERT_EQ("", context_id);
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  ASSERT_EQ("100", context_id);

  base::Value::Dict nav_params;
  nav_params.SetByDottedPath("frame.parentId", "1");
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Page.frameNavigated", nav_params).code());
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  nav_params.clear();
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Page.frameNavigated", nav_params).code());
  ASSERT_EQ(kNoSuchExecutionContext,
            tracker.GetContextIdForFrame("f", &context_id).code());
}

TEST(FrameTracker, AuxData) {
  StubDevToolsClient client;
  FrameTracker tracker(&client);
  std::string context_id;
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsError());
  ASSERT_EQ("", context_id);

  const char context[] = "{\"uniqueId\":\"100\",\"auxData\":{}}";
  base::Value::Dict params;
  params.Set("context", base::test::ParseJson(context));
  params.SetByDottedPath("context.auxData.frameId", "f");
  params.SetByDottedPath("context.auxData.isDefault", true);
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Runtime.executionContextCreated", params)
                .code());
  ASSERT_EQ(kNoSuchExecutionContext,
            tracker.GetContextIdForFrame("foo", &context_id).code());
  ASSERT_EQ("", context_id);
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  ASSERT_EQ("100", context_id);
}

TEST(FrameTracker, CanUpdateFrameContextId) {
  StubDevToolsClient client;
  FrameTracker tracker(&client);

  const char context[] =
      "{\"uniqueId\":\"1\",\"auxData\":{\"frameId\":\"f\",\"isDefault\":true}}";
  base::Value::Dict params;
  params.Set("context", base::test::ParseJson(context));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Runtime.executionContextCreated", params)
                .code());
  std::string context_id;
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  ASSERT_EQ("1", context_id);

  params.SetByDottedPath("context.uniqueId", "2");
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Runtime.executionContextCreated", params)
                .code());
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  ASSERT_EQ("2", context_id);
}

TEST(FrameTracker, DontTrackContentScriptContexts) {
  StubDevToolsClient client;
  FrameTracker tracker(&client);

  const char context[] =
      "{\"uniqueId\":\"1\",\"auxData\":{\"frameId\":\"f\",\"isDefault\":true}}";
  base::Value::Dict params;
  params.Set("context", base::test::ParseJson(context));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Runtime.executionContextCreated", params)
                .code());
  std::string context_id;
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  ASSERT_EQ("1", context_id);

  params.SetByDottedPath("context.uniqueId", "2");
  params.SetByDottedPath("context.auxData.isDefault", false);
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Runtime.executionContextCreated", params)
                .code());
  ASSERT_TRUE(tracker.GetContextIdForFrame("f", &context_id).IsOk());
  ASSERT_EQ("1", context_id);
}

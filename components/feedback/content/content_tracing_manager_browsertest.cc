// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "components/feedback/tracing_manager.h"
#include "components/feedback/content/content_tracing_manager.h"

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContentTracingManagerBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
  }
};

// Test that the trace data is compressed successfully.
IN_PROC_BROWSER_TEST_F(ContentTracingManagerBrowserTest, TraceDataCompressed) {
  std::unique_ptr<ContentTracingManager> manager =
      ContentTracingManager::Create();
  EXPECT_NE(manager, nullptr);

  base::test::TestFuture<scoped_refptr<base::RefCountedString>> future;

  int trace_id = manager->RequestTrace();
  EXPECT_EQ(trace_id, 1);
  manager->GetTraceData(trace_id, future.GetCallback());
  // The actual trace data varies.
  EXPECT_TRUE(future.Get()->data().size() > 0);
}

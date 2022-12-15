// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_cache {

TEST(WebCacheManagerTest, AddRemoveRendererTest) {
  content::BrowserTaskEnvironment task_environment_;
  WebCacheManager manager;
  const int kRendererID = 146;

  EXPECT_EQ(0U, manager.renderers_.size());

  manager.Add(kRendererID);
  EXPECT_EQ(1U, manager.renderers_.count(kRendererID));

  manager.Remove(kRendererID);
  EXPECT_EQ(0U, manager.renderers_.size());
}

}  // namespace web_cache

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/browser/web_cache_manager.h"

#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_cache {

using WebCacheManagerTest = content::RenderViewHostTestHarness;

TEST_F(WebCacheManagerTest, AddRemoveRendererTest) {
  WebCacheManager manager;
  // The main_rfh's process is added during the constructor of
  // 'WebCacheManager'.
  EXPECT_EQ(1U, manager.web_cache_services_.size());

  manager.Remove(process());
  EXPECT_EQ(0U, manager.web_cache_services_.size());

  manager.Add(process());
  EXPECT_EQ(1U, manager.web_cache_services_.size());

  manager.Remove(process());
  EXPECT_EQ(0U, manager.web_cache_services_.size());
}

}  // namespace web_cache

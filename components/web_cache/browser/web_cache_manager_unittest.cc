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
  EXPECT_EQ(0U, manager.web_cache_services_.size());

  manager.Add(process());
  EXPECT_EQ(1U, manager.web_cache_services_.size());

  manager.Remove(process());
  EXPECT_EQ(0U, manager.web_cache_services_.size());
}

TEST_F(WebCacheManagerTest, OnlyClearCacheWhenProcessReady) {
  WebCacheManager manager;
  auto renderer =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  renderer->Init();
  EXPECT_FALSE(renderer->IsReady());
  EXPECT_FALSE(manager.rph_observations_.IsObservingSource(renderer.get()));
  EXPECT_EQ(manager.web_cache_services_.find(renderer->GetID()),
            manager.web_cache_services_.end());

  // Simulate the creation of a process host to ensure 'manager' observer this
  // 'RenderProcessHost'.
  manager.OnRenderProcessHostCreated(renderer.get());

  EXPECT_FALSE(renderer->IsReady());
  EXPECT_TRUE(manager.rph_observations_.IsObservingSource(renderer.get()));
  EXPECT_FALSE(manager.web_cache_services_.contains(renderer->GetID()));

  renderer->SimulateReady();

  EXPECT_TRUE(renderer->IsReady());
  EXPECT_TRUE(manager.rph_observations_.IsObservingSource(renderer.get()));
  EXPECT_TRUE(manager.web_cache_services_.contains(renderer->GetID()));
}

TEST_F(WebCacheManagerTest, PreExistingRenderProcessHost) {
  std::unique_ptr<content::MockRenderProcessHost> pre_renderer_not_ready =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  pre_renderer_not_ready->Init();

  std::unique_ptr<content::MockRenderProcessHost> pre_renderer_ready =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  pre_renderer_ready->Init();
  pre_renderer_ready->SimulateReady();

  // RenderProcessHosts that are created before the WebCacheManager must also be
  // observed.
  WebCacheManager manager;

  EXPECT_FALSE(pre_renderer_not_ready->IsReady());
  EXPECT_TRUE(manager.rph_observations_.IsObservingSource(
      pre_renderer_not_ready.get()));
  EXPECT_FALSE(
      manager.web_cache_services_.contains(pre_renderer_not_ready->GetID()));

  EXPECT_TRUE(pre_renderer_ready->IsReady());
  EXPECT_TRUE(
      manager.rph_observations_.IsObservingSource(pre_renderer_ready.get()));
  EXPECT_TRUE(
      manager.web_cache_services_.contains(pre_renderer_ready->GetID()));
}

}  // namespace web_cache

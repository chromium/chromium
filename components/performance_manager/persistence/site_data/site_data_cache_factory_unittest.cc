// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/sequence_bound.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(SiteDataCacheFactoryTest, EndToEnd) {
  content::BrowserTaskEnvironment task_environment;
  auto performance_manager = PerformanceManagerImpl::Create(base::DoNothing());
  base::SequenceBound<SiteDataCacheFactory> cache_factory(
      PerformanceManager::GetTaskRunner());

  content::TestBrowserContext browser_context;
  cache_factory.AsyncCall(&SiteDataCacheFactory::OnBrowserContextCreated)
      .WithArgs(browser_context.UniqueId(), browser_context.GetPath(),
                std::nullopt);

  {
    base::RunLoop run_loop;
    cache_factory.PostTaskWithThisObject(
        base::BindOnce(
            [](const std::string& browser_context_id,
               base::OnceClosure quit_closure, SiteDataCacheFactory* factory) {
              EXPECT_TRUE(factory);
              EXPECT_NE(nullptr, factory->GetDataCacheForBrowserContext(
                                     browser_context_id));
              EXPECT_NE(nullptr, factory->GetInspectorForBrowserContext(
                                     browser_context_id));
              std::move(quit_closure).Run();
            },
            browser_context.UniqueId(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  cache_factory.AsyncCall(&SiteDataCacheFactory::OnBrowserContextDestroyed)
      .WithArgs(browser_context.UniqueId());
  {
    base::RunLoop run_loop;
    cache_factory.PostTaskWithThisObject(
        base::BindOnce(
            [](const std::string& browser_context_id,
               base::OnceClosure quit_closure, SiteDataCacheFactory* factory) {
              EXPECT_EQ(nullptr, factory->GetDataCacheForBrowserContext(
                                     browser_context_id));
              EXPECT_EQ(nullptr, factory->GetInspectorForBrowserContext(
                                     browser_context_id));
              std::move(quit_closure).Run();
            },
            browser_context.UniqueId(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  PerformanceManagerImpl::Destroy(std::move(performance_manager));
  task_environment.RunUntilIdle();
}

}  // namespace performance_manager

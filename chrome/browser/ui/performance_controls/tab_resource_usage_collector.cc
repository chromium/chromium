// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "content/public/browser/web_contents.h"

namespace {
constexpr base::TimeDelta kTabResourceUsageRefreshInterval = base::Minutes(2);

using resource_attribution::MemorySummaryResult;
using resource_attribution::PageContext;
using resource_attribution::QueryBuilder;
using resource_attribution::QueryResultMap;
using resource_attribution::ResourceType;
}  // namespace

TabResourceUsageCollector::TabResourceUsageCollector()
    : scoped_query_(QueryBuilder()
                        .AddAllContextsOfType<PageContext>()
                        .AddResourceType(ResourceType::kMemorySummary)
                        .CreateScopedQuery()) {
  query_observer_.Observe(&scoped_query_);
  scoped_query_.Start(kTabResourceUsageRefreshInterval);
  load_state_observer_.Observe(resource_coordinator::TabLoadTracker::Get());
}

TabResourceUsageCollector::~TabResourceUsageCollector() = default;

TabResourceUsageCollector* TabResourceUsageCollector::Get() {
  static base::NoDestructor<TabResourceUsageCollector> collector;
  return collector.get();
}

void TabResourceUsageCollector::AddObserver(Observer* o) {
  observers_.AddObserver(o);
}

void TabResourceUsageCollector::RemoveObserver(Observer* o) {
  observers_.RemoveObserver(o);
}

void TabResourceUsageCollector::ImmediatelyRefreshMetrics(
    content::WebContents* web_contents) {
  std::optional<PageContext> page_context =
      PageContext::FromWebContents(web_contents);
  if (page_context.has_value()) {
    QueryBuilder()
        .AddResourceType(ResourceType::kMemorySummary)
        .AddResourceContext(page_context.value())
        .QueryOnce(
            base::BindOnce(&TabResourceUsageCollector::OnResourceUsageUpdated,
                           base::Unretained(Get())));
  }
}

void TabResourceUsageCollector::ImmediatelyRefreshMetricsForAllTabs() {
  QueryBuilder()
      .AddResourceType(ResourceType::kMemorySummary)
      .AddAllContextsOfType<PageContext>()
      .QueryOnce(
          base::BindOnce(&TabResourceUsageCollector::OnResourceUsageUpdated,
                         base::Unretained(Get())));
}

void TabResourceUsageCollector::OnResourceUsageUpdated(
    const QueryResultMap& results) {
  bool did_resource_update = false;
  for (const auto& [page_context, result] : results) {
    std::optional<MemorySummaryResult> memory_result =
        result.memory_summary_result;
    if (memory_result.has_value()) {
      content::WebContents* const web_contents =
          resource_attribution::AsContext<PageContext>(page_context)
              .GetWebContents();
      if (web_contents) {
        auto* const tab_resource_usage_tab_helper =
            TabResourceUsageTabHelper::FromWebContents(web_contents);
        if (tab_resource_usage_tab_helper) {
          tab_resource_usage_tab_helper->SetMemoryUsageInBytes(
              memory_result->private_footprint_kb * 1024);
          did_resource_update = true;
        }
      }
    }
  }

  if (did_resource_update) {
    for (auto& obs : observers_) {
      obs.OnTabResourceMetricsRefreshed();
    }
  }
}

void TabResourceUsageCollector::OnLoadingStateChange(
    content::WebContents* web_contents,
    LoadingState old_loading_state,
    LoadingState new_loading_state) {
  if (new_loading_state == LoadingState::LOADED) {
    ImmediatelyRefreshMetrics(web_contents);
  }
}

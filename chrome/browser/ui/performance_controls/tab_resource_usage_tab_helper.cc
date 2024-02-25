// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/navigation_handle.h"

void TabResourceUsage::SetMemoryUsageInBytes(uint64_t memory_usage_bytes) {
  memory_usage_bytes_ = memory_usage_bytes;
  is_high_memory_usage_ =
      memory_usage_bytes_ >
      base::checked_cast<uint64_t>(
          performance_manager::features::
              kMemoryUsageInHovercardsHighThresholdBytes.Get());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabResourceUsageTabHelper);

TabResourceUsageTabHelper::~TabResourceUsageTabHelper() = default;

TabResourceUsageTabHelper::TabResourceUsageTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<TabResourceUsageTabHelper>(*contents),
      resource_usage_(base::MakeRefCounted<TabResourceUsage>()) {}

void TabResourceUsageTabHelper::PrimaryPageChanged(content::Page&) {
  // Reset memory usage count when we navigate to another site since the
  // memory usage reported will be outdated.
  resource_usage_->SetMemoryUsageInBytes(0);
}

void TabResourceUsageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (performance_manager::features::kMemoryUsageInHovercardsUpdateTrigger
              .Get() == performance_manager::features::
                            MemoryUsageInHovercardsUpdateTrigger::kNavigation &&
      navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    TabResourceUsageCollector::Get()->ImmediatelyRefreshMetrics(web_contents());
  }
}

uint64_t TabResourceUsageTabHelper::GetMemoryUsageInBytes() {
  return resource_usage_->memory_usage_in_bytes();
}

void TabResourceUsageTabHelper::SetMemoryUsageInBytes(
    uint64_t memory_usage_bytes) {
  resource_usage_->SetMemoryUsageInBytes(memory_usage_bytes);
}

scoped_refptr<const TabResourceUsage>
TabResourceUsageTabHelper::resource_usage() const {
  return resource_usage_;
}

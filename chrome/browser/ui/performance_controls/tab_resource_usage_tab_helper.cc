// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "base/byte_count.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(TabResourceUsageTabHelper);

TabResourceUsage::TabResourceUsage() = default;

void TabResourceUsage::SetMemoryUsage(base::ByteCount memory_usage) {
  memory_usage_ = memory_usage;
  is_high_memory_usage_ = memory_usage_ > kHighMemoryUsageThreshold;
}

TabResourceUsageTabHelper::~TabResourceUsageTabHelper() = default;

TabResourceUsageTabHelper::TabResourceUsageTabHelper(tabs::TabInterface& tab)
    : ContentsObservingTabFeature(tab),
      resource_usage_(base::MakeRefCounted<TabResourceUsage>()),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

TabResourceUsageTabHelper* TabResourceUsageTabHelper::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void TabResourceUsageTabHelper::PrimaryPageChanged(content::Page&) {
  // Reset memory usage count when we navigate to another site since the
  // memory usage reported will be outdated.
  resource_usage_->SetMemoryUsage(base::ByteCount(0));
  NotifyResourceUsageChanged();
}

base::ByteCount TabResourceUsageTabHelper::GetMemoryUsage() {
  return resource_usage_->memory_usage();
}

void TabResourceUsageTabHelper::SetMemoryUsage(base::ByteCount memory_usage) {
  resource_usage_->SetMemoryUsage(memory_usage);
  NotifyResourceUsageChanged();
}

scoped_refptr<const TabResourceUsage>
TabResourceUsageTabHelper::resource_usage() const {
  return resource_usage_;
}

base::CallbackListSubscription
TabResourceUsageTabHelper::AddResourceUsageChangeCallback(
    ResourceChangedCallback callback) {
  return callback_list_.Add(callback);
}

void TabResourceUsageTabHelper::NotifyResourceUsageChanged() {
  callback_list_.Notify(resource_usage());
}

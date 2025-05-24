// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/performance_manager/public/features.h"

namespace tabs {
class TabFeatures;
class TabInterface;
}  // namespace tabs

class TabResourceUsage : public base::RefCounted<TabResourceUsage> {
 public:
  // Threshold was selected based on the 99th percentile of tab memory usage
  static const uint64_t kHighMemoryUsageThresholdBytes = 800 * 1024 * 1024;

  TabResourceUsage() = default;

  uint64_t memory_usage_in_bytes() const { return memory_usage_bytes_; }

  void SetMemoryUsageInBytes(uint64_t memory_usage_bytes);

  bool is_high_memory_usage() const { return is_high_memory_usage_; }

 private:
  friend class base::RefCounted<TabResourceUsage>;
  ~TabResourceUsage() = default;

  uint64_t memory_usage_bytes_ = 0;
  bool is_high_memory_usage_ = false;
};

// Per-tab class to keep track of current memory usage for each tab.
class TabResourceUsageTabHelper : public tabs::ContentsObservingTabFeature {
 public:
  explicit TabResourceUsageTabHelper(tabs::TabInterface& contents);
  ~TabResourceUsageTabHelper() override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  uint64_t GetMemoryUsageInBytes();
  void SetMemoryUsageInBytes(uint64_t memory_usage_bytes);

  scoped_refptr<const TabResourceUsage> resource_usage() const;

 private:
  friend class tabs::TabFeatures;

  scoped_refptr<TabResourceUsage> resource_usage_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_

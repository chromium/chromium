// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_

#include "base/byte_count.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/performance_manager/public/features.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabFeatures;
class TabInterface;
}  // namespace tabs

class TabResourceUsage : public base::RefCounted<TabResourceUsage> {
 public:
  // Threshold was selected based on the 99th percentile of tab memory usage.
  static constexpr base::ByteCount kHighMemoryUsageThreshold = base::MiB(800);

  TabResourceUsage();

  base::ByteCount memory_usage() const { return memory_usage_; }

  void SetMemoryUsage(base::ByteCount memory_usage);

  bool is_high_memory_usage() const { return is_high_memory_usage_; }

 private:
  friend class base::RefCounted<TabResourceUsage>;
  ~TabResourceUsage() = default;

  base::ByteCount memory_usage_;
  bool is_high_memory_usage_ = false;
};

// Per-tab class to keep track of current memory usage for each tab.
class TabResourceUsageTabHelper : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(TabResourceUsageTabHelper);
  explicit TabResourceUsageTabHelper(tabs::TabInterface& contents);
  ~TabResourceUsageTabHelper() override;

  static TabResourceUsageTabHelper* From(tabs::TabInterface* tab);

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  base::ByteCount GetMemoryUsage();
  void SetMemoryUsage(base::ByteCount memory_usage);

  scoped_refptr<const TabResourceUsage> resource_usage() const;

  // Registers callback if tab's resource usage is updated
  using ResourceChangedCallback =
      base::RepeatingCallback<void(scoped_refptr<const TabResourceUsage>)>;
  base::CallbackListSubscription AddResourceUsageChangeCallback(
      ResourceChangedCallback callback);

 private:
  friend class tabs::TabFeatures;

  void NotifyResourceUsageChanged();

  base::RepeatingCallbackList<void(scoped_refptr<const TabResourceUsage>)>
      callback_list_;
  scoped_refptr<TabResourceUsage> resource_usage_;
  ui::ScopedUnownedUserData<TabResourceUsageTabHelper>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class TabResourceUsage : public base::RefCounted<TabResourceUsage> {
 public:
  TabResourceUsage() = default;

  uint64_t memory_usage_in_bytes() const { return memory_usage_bytes_; }

  void set_memory_usage_in_bytes(uint64_t memory_usage_bytes) {
    memory_usage_bytes_ = memory_usage_bytes;
  }

 private:
  friend class base::RefCounted<TabResourceUsage>;
  ~TabResourceUsage() = default;

  uint64_t memory_usage_bytes_ = 0;
};

// Per-tab class to keep track of current memory usage for each tab.
class TabResourceUsageTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabResourceUsageTabHelper> {
 public:
  TabResourceUsageTabHelper(const TabResourceUsageTabHelper&) = delete;
  TabResourceUsageTabHelper& operator=(const TabResourceUsageTabHelper&) =
      delete;

  ~TabResourceUsageTabHelper() override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  uint64_t GetMemoryUsageInBytes();
  void SetMemoryUsageInBytes(uint64_t memory_usage_bytes);

  scoped_refptr<const TabResourceUsage> resource_usage() const;

 private:
  friend class content::WebContentsUserData<TabResourceUsageTabHelper>;
  explicit TabResourceUsageTabHelper(content::WebContents* contents);
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  scoped_refptr<TabResourceUsage> resource_usage_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_RESOURCE_USAGE_TAB_HELPER_H_

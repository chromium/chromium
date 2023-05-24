// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_RESOURCE_USAGE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_RESOURCE_USAGE_TAB_HELPER_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// Per-tab class to keep track of current memory usage for each tab.
class ResourceUsageTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ResourceUsageTabHelper> {
 public:
  ResourceUsageTabHelper(const ResourceUsageTabHelper&) = delete;
  ResourceUsageTabHelper& operator=(const ResourceUsageTabHelper&) = delete;

  ~ResourceUsageTabHelper() override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  uint64_t GetMemoryUsageInBytes() { return memory_usage_bytes_; }

  void SetMemoryUsageInBytes(uint64_t memory_usage_bytes) {
    memory_usage_bytes_ = memory_usage_bytes;
  }

 private:
  friend class content::WebContentsUserData<ResourceUsageTabHelper>;
  explicit ResourceUsageTabHelper(content::WebContents* contents);

  uint64_t memory_usage_bytes_ = 0;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_RESOURCE_USAGE_TAB_HELPER_H_

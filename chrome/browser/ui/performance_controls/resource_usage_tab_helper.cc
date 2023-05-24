// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/resource_usage_tab_helper.h"

#include "content/public/browser/web_contents.h"

ResourceUsageTabHelper::~ResourceUsageTabHelper() = default;

void ResourceUsageTabHelper::PrimaryPageChanged(content::Page&) {
  // Reset memory usage count when we navigate to another site since the
  // memory usage reported will be outdated.
  memory_usage_bytes_ = 0;
}

ResourceUsageTabHelper::ResourceUsageTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<ResourceUsageTabHelper>(*contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceUsageTabHelper);

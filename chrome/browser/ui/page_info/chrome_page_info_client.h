// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_CLIENT_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_CLIENT_H_

#include "components/page_info/android/page_info_client.h"
#include "components/page_info/page_info_delegate.h"
#include "components/page_info/page_info_ui_delegate.h"

#include <memory>

namespace content {
class WebContents;
}

// Chrome's implementation of PageInfoClient. Allows //components/page_info
// classes to retrieve PageInfo delegates, which override //components logic.
class ChromePageInfoClient : public page_info::PageInfoClient {
 public:
  ChromePageInfoClient() = default;
  ~ChromePageInfoClient() = default;

  std::unique_ptr<PageInfoDelegate> CreatePageInfoDelegate(
      content::WebContents* web_contents) override;
  int GetJavaResourceId(int native_resource_id) override;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_CLIENT_H_

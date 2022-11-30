// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/android/page_info_client.h"

#include "content/public/browser/web_contents.h"

namespace page_info {

static PageInfoClient* g_client;

void SetPageInfoClient(PageInfoClient* client) {
  g_client = client;
}

PageInfoClient* GetPageInfoClient() {
  return g_client;
}

std::unique_ptr<PageInfoDelegate> PageInfoClient::CreatePageInfoDelegate(
    content::WebContents* web_contents) {
  return nullptr;
}

int PageInfoClient::GetJavaResourceId(int native_resource_id) {
  return -1;
}

}  // namespace page_info

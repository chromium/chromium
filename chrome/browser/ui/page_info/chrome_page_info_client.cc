// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_client.h"

#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "content/public/browser/web_contents.h"

std::unique_ptr<PageInfoDelegate> ChromePageInfoClient::CreatePageInfoDelegate(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return std::make_unique<ChromePageInfoDelegate>(web_contents);
}

int ChromePageInfoClient::GetJavaResourceId(int native_resource_id) {
  return ResourceMapper::MapToJavaDrawableId(native_resource_id);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/async_web_contents_destroyer_data.h"

#include "content/public/browser/web_contents.h"

namespace tabs {

// static
bool AsyncWebContentsDestroyerData::WillBeDestroyed(
    const content::WebContents* web_contents) {
  return web_contents && FromWebContents(web_contents) != nullptr;
}

AsyncWebContentsDestroyerData::AsyncWebContentsDestroyerData(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AsyncWebContentsDestroyerData>(
          *web_contents) {}

AsyncWebContentsDestroyerData::~AsyncWebContentsDestroyerData() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(AsyncWebContentsDestroyerData);

}  // namespace tabs

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/web_contents_wrapper.h"

#include "content/public/browser/browser_context.h"

namespace commerce {

WebContentsWrapper::WebContentsWrapper(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

const GURL& WebContentsWrapper::GetLastCommittedURL() {
  if (!web_contents_)
    return std::move(GURL());

  return web_contents_->GetLastCommittedURL();
}

bool WebContentsWrapper::IsOffTheRecord() {
  if (!web_contents_ || !web_contents_->GetBrowserContext())
    return false;

  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsWrapper::ClearWebContentsPointer() {
  web_contents_ = nullptr;
}

}  // namespace commerce

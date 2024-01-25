// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/web_contents_proxy.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

WebContentsProxy::WebContentsProxy() = default;
WebContentsProxy::WebContentsProxy(const WebContentsProxy& other) = default;
WebContentsProxy::WebContentsProxy(WebContentsProxy&& other) = default;

WebContentsProxy::~WebContentsProxy() = default;

WebContentsProxy& WebContentsProxy::operator=(const WebContentsProxy& other) =
    default;
WebContentsProxy& WebContentsProxy::operator=(WebContentsProxy&& other) =
    default;

content::WebContents* WebContentsProxy::Get() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return web_contents_.get();
}

WebContentsProxy::WebContentsProxy(
    base::WeakPtr<content::WebContents> web_contents)
    : web_contents_(std::move(web_contents)) {}

}  // namespace performance_manager

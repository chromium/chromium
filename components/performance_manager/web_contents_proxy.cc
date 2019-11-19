// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/web_contents_proxy.h"

#include "components/performance_manager/web_contents_proxy_impl.h"

namespace performance_manager {

WebContentsProxy::WebContentsProxy() = default;

WebContentsProxy::WebContentsProxy(const WebContentsProxy& other)
    : impl_(other.impl_) {}

WebContentsProxy::WebContentsProxy(WebContentsProxy&& other)
    : impl_(std::move(other.impl_)) {}

WebContentsProxy::~WebContentsProxy() = default;

WebContentsProxy& WebContentsProxy::operator=(const WebContentsProxy& other) {
  impl_ = other.impl_;
  return *this;
}

WebContentsProxy& WebContentsProxy::operator=(WebContentsProxy&& other) {
  impl_ = std::move(other.impl_);
  return *this;
}

content::WebContents* WebContentsProxy::Get() const {
  auto* proxy = impl_.get();
  if (!proxy)
    return nullptr;
  return proxy->GetWebContents();
}

int64_t WebContentsProxy::LastNavigationId() const {
  auto* proxy = impl_.get();
  if (!proxy)
    return 0;
  return proxy->LastNavigationId();
}

WebContentsProxy::WebContentsProxy(
    const base::WeakPtr<WebContentsProxyImpl>& impl)
    : impl_(impl) {}

}  // namespace performance_manager

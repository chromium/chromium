// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/content_browser_client_mixins_impl.h"

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace cast_receiver {

// static
std::unique_ptr<ContentBrowserClientMixins> ContentBrowserClientMixins::Create(
    network::NetworkContextGetter network_content_getter) {
  return std::make_unique<ContentBrowserClientMixinsImpl>(
      std::move(network_content_getter));
}

ContentBrowserClientMixinsImpl::ContentBrowserClientMixinsImpl(
    network::NetworkContextGetter network_context_getter)
    : application_client_(std::move(network_context_getter)) {}

ContentBrowserClientMixinsImpl::~ContentBrowserClientMixinsImpl() = default;

void ContentBrowserClientMixinsImpl::AddStreamingResolutionObserver(
    StreamingResolutionObserver* observer) {
  application_client_.AddStreamingResolutionObserver(observer);
}

void ContentBrowserClientMixinsImpl::RemoveStreamingResolutionObserver(
    StreamingResolutionObserver* observer) {
  application_client_.RemoveStreamingResolutionObserver(observer);
}

void ContentBrowserClientMixinsImpl::AddApplicationStateObserver(
    ApplicationStateObserver* observer) {
  application_client_.AddApplicationStateObserver(observer);
}

void ContentBrowserClientMixinsImpl::RemoveApplicationStateObserver(
    ApplicationStateObserver* observer) {
  application_client_.RemoveApplicationStateObserver(observer);
}

void ContentBrowserClientMixinsImpl::OnWebContentsCreated(
    content::WebContents* web_contents) {
  application_client_.OnWebContentsCreated(web_contents);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ContentBrowserClientMixinsImpl::CreateURLLoaderThrottles(
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    CorsExemptHeaderCallback is_cors_exempt_header_cb) {
  return application_client_.CreateURLLoaderThrottles(
      std::move(wc_getter), frame_tree_node_id,
      std::move(is_cors_exempt_header_cb));
}

ApplicationClient& ContentBrowserClientMixinsImpl::GetApplicationClient() {
  return application_client_;
}

}  // namespace cast_receiver

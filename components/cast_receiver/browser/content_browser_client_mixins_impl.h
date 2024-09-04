// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_CONTENT_BROWSER_CLIENT_MIXINS_IMPL_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_CONTENT_BROWSER_CLIENT_MIXINS_IMPL_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/cast_receiver/browser/application_client.h"
#include "components/cast_receiver/browser/public/content_browser_client_mixins.h"

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

// This class acts as a wrapper around an ApplicationClient. It exists only to
// avoid a circular dependency in header files if ApplicationClient implements
// ContentBrowserClientMixins directly:
//
// ContentBrowserClientMixins -> RuntimeApplicationDispatcherImpl ->
// ApplicationClient -> ContentBrowserClientMixins.
//
// All function implementations delegate to those of ApplicationClient.
class ContentBrowserClientMixinsImpl : public ContentBrowserClientMixins {
 public:
  explicit ContentBrowserClientMixinsImpl(
      network::NetworkContextGetter network_context_getter);
  ~ContentBrowserClientMixinsImpl() override;

  // ContentBrowserClientMixins implementation.
  void AddApplicationStateObserver(ApplicationStateObserver* observer) override;
  void RemoveApplicationStateObserver(
      ApplicationStateObserver* observer) override;
  void AddStreamingResolutionObserver(
      StreamingResolutionObserver* observer) override;
  void RemoveStreamingResolutionObserver(
      StreamingResolutionObserver* observer) override;
  void OnWebContentsCreated(content::WebContents* web_contents) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      CorsExemptHeaderCallback is_cors_exempt_header_cb) override;
  ApplicationClient& GetApplicationClient() override;

 private:
  ApplicationClient application_client_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_CONTENT_BROWSER_CLIENT_MIXINS_IMPL_H_

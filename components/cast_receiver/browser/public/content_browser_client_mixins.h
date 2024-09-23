// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_CONTENT_BROWSER_CLIENT_MIXINS_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_CONTENT_BROWSER_CLIENT_MIXINS_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "components/cast_receiver/browser/runtime_application_dispatcher_impl.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

class ApplicationClient;
class ApplicationStateObserver;
class StreamingResolutionObserver;

// This class is responsible for providing all factory methods required for
// creating the classes responsible for management and control of cast
// application types, as required for the functionality of the remainder of
// this component, as well as responding to any callbacks from the application
// process.
class ContentBrowserClientMixins {
 public:
  // The NetworkContext to use with the cast_streaming component for network
  // access to implement the Cast Streaming receiver. This NetworkContext is
  // eventually passed to the Open Screen library platform implementation.
  static std::unique_ptr<ContentBrowserClientMixins> Create(
      network::NetworkContextGetter network_context_getter);

  virtual ~ContentBrowserClientMixins() = default;

  // Adds or removes a ApplicationStateObserver. |observer| must not yet have
  // been added, must be non-null, and is expected to remain valid for the
  // duration of this instance's lifetime or until the associated Remove method
  // below is called for a call to AddApplicationStateObserver(), and must
  // have been previously added for a call to RemoveApplicationStateObserver().
  virtual void AddApplicationStateObserver(
      ApplicationStateObserver* observer) = 0;
  virtual void RemoveApplicationStateObserver(
      ApplicationStateObserver* observer) = 0;

  // Adds or removes a StreamingResolutionObserver. |observer| must not yet have
  // been added, must be non-null, and is expected to remain valid for the
  // duration of this instance's lifetime or until the associated Remove method
  // below is called for a call to AddStreamingResolutionObserver(), and must
  // have been previously added for a call to
  // RemoveStreamingResolutionObserver().
  virtual void AddStreamingResolutionObserver(
      StreamingResolutionObserver* observer) = 0;
  virtual void RemoveStreamingResolutionObserver(
      StreamingResolutionObserver* observer) = 0;

  // To be called for every new WebContents creation.
  virtual void OnWebContentsCreated(content::WebContents* web_contents) = 0;

  // To be called by the ContentBrowserClient function of the same name.
  using CorsExemptHeaderCallback =
      base::RepeatingCallback<bool(std::string_view)>;
  virtual std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      CorsExemptHeaderCallback is_cors_exempt_header_cb) = 0;

  // Creates a new RuntimeApplicationDispatcher.
  //
  // |TEmbedderApplication| must implement EmbedderApplication.
  template <typename TEmbedderApplication>
  std::unique_ptr<RuntimeApplicationDispatcher<TEmbedderApplication>>
  CreateApplicationDispatcher() {
    return std::make_unique<
        RuntimeApplicationDispatcherImpl<TEmbedderApplication>>(
        GetApplicationClient());
  }

 protected:
  virtual ApplicationClient& GetApplicationClient() = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_CONTENT_BROWSER_CLIENT_MIXINS_H_

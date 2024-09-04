// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_APPLICATION_CLIENT_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_APPLICATION_CLIENT_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/cast_receiver/browser/public/application_state_observer.h"
#include "components/cast_receiver/browser/public/streaming_resolution_observer.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/cpp/network_context_getter.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
struct VideoTransformation;
}  // namespace media

namespace media_control {
class MediaBlocker;
}  // namespace media_control

namespace url_rewrite {
class UrlRequestRewriteRulesManager;
}  // namespace url_rewrite

namespace cast_receiver {

class RuntimeApplication;

// This class is responsible for providing all factory methods required for
// creating the classes responsible for management and control of cast
// application types, as required for the functionality of the remainder of
// this component, as well as responding to any callbacks from the application
// process.
class ApplicationClient : public StreamingResolutionObserver,
                          public ApplicationStateObserver {
 public:
  // This class handles managing the lifetime and interaction with the Renderer
  // process for application-specific objects. All functions of this object are
  // safe to call at any point during this object's lifetime.
  class ApplicationControls {
   public:
    virtual ~ApplicationControls();

    // Returns the MediaBlocker instance associated with this application.
    virtual media_control::MediaBlocker& GetMediaBlocker() = 0;

    // Returns the UrlRequestRewriteRulesManager instance associated with this
    // application.
    virtual url_rewrite::UrlRequestRewriteRulesManager&
    GetUrlRequestRewriteRulesManager() = 0;
  };
  explicit ApplicationClient(
      network::NetworkContextGetter network_context_getter);
  ~ApplicationClient() override;

  // Returns the NetworkContext to use with the cast_streaming component for
  // network access to implement the Cast Streaming receiver.  (This
  // NetworkContext is eventually passed to the Open Screen library platform
  // implementation.)
  network::NetworkContextGetter network_context_getter() const {
    return network_context_getter_;
  }

  // Returns the ApplicationControls associated with |web_contents|. The
  // lifetime of this instance is the same as that of |web_contents|.
  ApplicationControls& GetApplicationControls(
      const content::WebContents& web_contents);

  // As defined in ContentBrowserClientMixins.
  void AddApplicationStateObserver(ApplicationStateObserver* observer);
  void RemoveApplicationStateObserver(ApplicationStateObserver* observer);
  void AddStreamingResolutionObserver(StreamingResolutionObserver* observer);
  void RemoveStreamingResolutionObserver(StreamingResolutionObserver* observer);
  void OnWebContentsCreated(content::WebContents* web_contents);
  using CorsExemptHeaderCallback =
      base::RepeatingCallback<bool(std::string_view)>;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      CorsExemptHeaderCallback is_cors_exempt_header_cb);

  // StreamingResolutionObserver implementation:
  void OnStreamingResolutionChanged(
      const gfx::Rect& size,
      const media::VideoTransformation& transformation) final;

  // ApplicationStateObserver implementation:
  void OnForegroundApplicationChanged(RuntimeApplication* app) final;

 private:
  network::NetworkContextGetter network_context_getter_;

  base::ObserverList<StreamingResolutionObserver>
      streaming_resolution_observer_list_;
  base::ObserverList<ApplicationStateObserver> application_state_observer_list_;

  base::WeakPtrFactory<ApplicationClient> weak_factory_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_APPLICATION_CLIENT_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CLIENT_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CLIENT_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/cast_receiver/browser/public/application_state_observer.h"
#include "components/cast_receiver/browser/public/streaming_resolution_observer.h"

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

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

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
  };

  ApplicationClient();
  ~ApplicationClient() override;

  // Returns the ApplicationControls associated with |web_contents|. The
  // lifetime of this instance is the same as that of |web_contents|.
  ApplicationControls& GetApplicationControls(
      const content::WebContents& web_contents);

  // Adds or removes a ApplicationStateObserver. |observer| must not yet have
  // been added, must be non-null, and is expected to remain valid for the
  // duration of this instance's lifetime or until the associated Remove method
  // below is called for a call to AddApplicationStateObserver(), and must
  // have been previously added for a call to RemoveApplicationStateObserver().
  void AddApplicationStateObserver(ApplicationStateObserver* observer);
  void RemoveApplicationStateObserver(ApplicationStateObserver* observer);

  // Adds or removes a StreamingResolutionObserver. |observer| must not yet have
  // been added, must be non-null, and is expected to remain valid for the
  // duration of this instance's lifetime or until the associated Remove method
  // below is called for a call to AddStreamingResolutionObserver(), and must
  // have been previously added for a call to
  // RemoveStreamingResolutionObserver().
  void AddStreamingResolutionObserver(StreamingResolutionObserver* observer);
  void RemoveStreamingResolutionObserver(StreamingResolutionObserver* observer);

  // Returns the NetworkContext to use with the cast_streaming component for
  // network access to implement the Cast Streaming receiver.  (This
  // NetworkContext is eventually passed to the Open Screen library platform
  // implementation.)
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;
  virtual NetworkContextGetter GetNetworkContextGetter() = 0;

  // To be called for every new WebContents creation.
  void OnWebContentsCreated(content::WebContents* web_contents);

  // StreamingResolutionObserver implementation:
  void OnStreamingResolutionChanged(
      const gfx::Rect& size,
      const media::VideoTransformation& transformation) final;

  // ApplicationStateObserver implementation:
  void OnForegroundApplicationChanged(RuntimeApplication* app) final;

 private:
  base::ObserverList<StreamingResolutionObserver>
      streaming_resolution_observer_list_;
  base::ObserverList<ApplicationStateObserver> application_state_observer_list_;

  base::WeakPtrFactory<ApplicationClient> weak_factory_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CLIENT_H_

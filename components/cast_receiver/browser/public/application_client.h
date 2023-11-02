// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CLIENT_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/cast_receiver/browser/public/application_state_observer.h"
#include "components/cast_receiver/browser/public/streaming_resolution_observer.h"

namespace chromecast {
class RuntimeApplication;
}  // namespace chromecast

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
struct VideoTransformation;
}  // namespace media

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace cast_receiver {

// This class is responsible for providing all factory methods required for
// creating the classes responsible for management and control of cast
// application types, as required for the functionality of the remainder of
// this component, as well as responding to any callbacks from the application
// process.
class ApplicationClient : public StreamingResolutionObserver,
                          public ApplicationStateObserver {
 public:
  ApplicationClient();
  ~ApplicationClient() override;

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

  // StreamingResolutionObserver implementation:
  void OnStreamingResolutionChanged(
      const gfx::Rect& size,
      const media::VideoTransformation& transformation) final;

  // ApplicationStateObserver implementation:
  void OnForegroundApplicationChanged(
      chromecast::RuntimeApplication* app) final;

 private:
  base::ObserverList<StreamingResolutionObserver>
      streaming_resolution_observer_list_;
  base::ObserverList<ApplicationStateObserver> application_state_observer_list_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CLIENT_H_

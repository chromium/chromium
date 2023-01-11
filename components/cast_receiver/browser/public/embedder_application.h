// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_EMBEDDER_APPLICATION_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_EMBEDDER_APPLICATION_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/cast_receiver/browser/public/runtime_application.h"

namespace content {
class WebContents;
class WebUIControllerFactory;
}  // namespace content

namespace cast_receiver {

class ContentWindowControls;
class MessagePortService;
class StreamingConfigManager;

// This class defines an interface to be implemented by embedders in order to
// allow a RuntimeApplication to interface with the embedder-specific details.
class EmbedderApplication {
 public:
  enum class ApplicationStopReason {
    kUndefined = 0,
    kApplicationRequest,
    kIdleTimeout,
    kUserRequest,
    kHttpError,
    kRuntimeError
  };

  virtual ~EmbedderApplication();

  // Notifies the Cast agent that application has started.
  virtual void NotifyApplicationStarted() = 0;

  // Notifies the Cast agent that application has stopped.
  virtual void NotifyApplicationStopped(ApplicationStopReason stop_reason,
                                        int32_t net_error_code) = 0;

  // Notifies the Cast agent about media playback state changed.
  virtual void NotifyMediaPlaybackChanged(bool playing) = 0;

  // Fetches all bindings asynchronously, calling |callback| with the results
  // of this call once it returns.
  using GetAllBindingsCallback =
      base::OnceCallback<void(cast_receiver::Status, std::vector<std::string>)>;
  virtual void GetAllBindings(GetAllBindingsCallback callback) = 0;

  // Gets the platform-specific MessagePortService instance for this
  // application, if such an instance exists.
  virtual MessagePortService* GetMessagePortService() = 0;

  // Returns the WebContents this application should use.
  //
  // TODO(crbug.com/1382907): Change to a callback-based API.
  virtual content::WebContents* GetWebContents() = 0;

  // Returns the window controls for this instance.
  //
  // TODO(crbug.com/1382907): Change to a callback-based API.
  virtual ContentWindowControls* GetContentWindowControls() = 0;

  // Returns the StreamingConfigManager to use for configuring a Cast Streaming
  // session. The default implementation returns a ReceiverConfig only
  // detailing the codec support as defined by the build flags.
  //
  // TODO(crbug.com/1382907): Change to a callback-based API.
  // TODO(crbug.com/1359568): Change default implementation to be based on
  // Chromium state.
  virtual StreamingConfigManager* GetStreamingConfigManager();

  // Creates a new platform-specific WebUIControllerFactory, or nullptr if
  // this feature is not to be supported. Returns nullptr by default.
  // |hosts| is the set of hosts for which the custom WebUIController associated
  // with the returned factory should be used.
  virtual std::unique_ptr<content::WebUIControllerFactory>
  CreateWebUIControllerFactory(std::vector<std::string> hosts);

  // Loads |url| in the associated WebContents.
  virtual void NavigateToPage(const GURL& url);
};

std::ostream& operator<<(std::ostream& os,
                         EmbedderApplication::ApplicationStopReason reason);

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_EMBEDDER_APPLICATION_H_

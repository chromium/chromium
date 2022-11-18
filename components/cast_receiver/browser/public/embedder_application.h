// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_EMBEDDER_APPLICATION_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_EMBEDDER_APPLICATION_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
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

  // Creates a new platform-specific WebUIControllerFactory.
  virtual std::unique_ptr<content::WebUIControllerFactory>
  CreateWebUIControllerFactory(std::vector<std::string> hosts) = 0;

  // Returns the WebContents this application should use.
  //
  // TODO(crbug.com/1382907): Change to a callback-based API.
  virtual content::WebContents* GetWebContents() = 0;

  // Returns the window controls for this instance.
  //
  // TODO(crbug.com/1382907): Change to a callback-based API.
  virtual ContentWindowControls* GetContentWindowControls() = 0;

  virtual StreamingConfigManager* GetStreamingConfigManager() = 0;

  // Loads |url| in the associated WebContents.
  //
  // TODO(crbug.com/1383332): Remove this function.
  virtual void LoadPage(const GURL& url) = 0;
};

std::ostream& operator<<(std::ostream& os,
                         EmbedderApplication::ApplicationStopReason reason);

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_EMBEDDER_APPLICATION_H_

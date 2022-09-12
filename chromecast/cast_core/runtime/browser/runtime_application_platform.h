// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_PLATFORM_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_PLATFORM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "components/cast_receiver/common/public/status.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/common/value.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/core_message_port_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_application_service.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/v2/runtime_message_port_application_service.castcore.pb.h"

namespace content {
class WebUIControllerFactory;
}

namespace chromecast {

class MessagePortService;

// This class defines a wrapper around any platform-specific communication
// details required for functionality of a RuntimeApplication.
class RuntimeApplicationPlatform {
 public:
  // Client used for executing commands in the runtime based on signals received
  // by the embedder implementing RuntimeApplicationPlatform.
  class Client {
   public:
    virtual ~Client() = default;

    // Sets the current URL Reqrite rules for this application.
    virtual void OnUrlRewriteRulesSet(
        url_rewrite::mojom::UrlRequestRewriteRulesPtr) = 0;

    // Sets the associated attribute of the content window.
    virtual void OnMediaStateSet(
        cast::common::MediaState::Type media_state) = 0;
    virtual void OnVisibilitySet(cast::common::Visibility::Type visibility) = 0;
    virtual void OnTouchInputSet(
        cast::common::TouchInput::Type touch_input) = 0;

    // Processes an incoming |message|, returning true on success and false on
    // failure.
    virtual bool OnMessagePortMessage(cast::web::Message message) = 0;

    // Returns information about the current state of the application.
    virtual bool IsApplicationRunning() = 0;
  };

  using Factory =
      base::OnceCallback<std::unique_ptr<RuntimeApplicationPlatform>(
          scoped_refptr<base::SequencedTaskRunner>,
          std::string,
          Client&)>;

  virtual ~RuntimeApplicationPlatform() = default;

  // Called before Launch() to perform any pre-launch loading that is
  // necessary. The |callback| will be called indicating if the operation
  // succeeded or not. If Load fails, |this| should be destroyed since it's not
  // necessarily valid to retry Load with a new |request|.
  using LoadCompleteCB = base::OnceCallback<void(cast_receiver::Status)>;
  virtual void Load(cast::runtime::LoadApplicationRequest request,
                    LoadCompleteCB callback) = 0;

  // Called to launch the application. The |callback| will be called indicating
  // whether the operation succeeded or not.
  using LaunchCompleteCB = base::OnceCallback<void(cast_receiver::Status)>;
  virtual void Launch(cast::runtime::LaunchApplicationRequest request,
                      LaunchCompleteCB callback) = 0;

  // Notifies the Cast agent that application has started.
  virtual void NotifyApplicationStarted() = 0;

  // Notifies the Cast agent that application has stopped.
  virtual void NotifyApplicationStopped(
      cast::common::StopReason::Type stop_reason,
      int32_t net_error_code) = 0;

  // Notifies the Cast agent about media playback state changed.
  virtual void NotifyMediaPlaybackChanged(bool playing) = 0;

  // Fetches all bindings asynchronously, calling |callback| with the results
  // of this call once it returns.
  using GetAllBindingsCB =
      base::OnceCallback<void(absl::optional<cast::bindings::GetAllResponse>)>;
  virtual void GetAllBindingsAsync(GetAllBindingsCB callback) = 0;

  // Creates a new platform-specific MessagePortService.
  virtual std::unique_ptr<MessagePortService> CreateMessagePortService() = 0;

  // Creates a new platform-specific WebUIControllerFactory.
  virtual std::unique_ptr<content::WebUIControllerFactory>
  CreateWebUIControllerFactory(std::vector<std::string> hosts) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_PLATFORM_H_

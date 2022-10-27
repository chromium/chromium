// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_

#include <string>

#include "base/callback.h"
#include "components/cast_receiver/browser/public/runtime_application_state.h"
#include "components/cast_receiver/common/public/status.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace content {
class WebUIControllerFactory;
}

namespace chromecast {

class MessagePortService;

// This represents an application that can be hosted by RuntimeService.  Its
// lifecycle is very simple: Load() -> Launch() -> Destruction.  Implementations
// of this interface will additionally communicate over various gRPC interfaces.
// For example, Launch needs to respond with SetApplicationStatus.
class RuntimeApplication : public cast_receiver::RuntimeApplicationState {
 public:
  using StatusCallback = base::OnceCallback<void(cast_receiver::Status)>;

  // This class defines a wrapper around any platform-specific communication
  // details required for functionality of a RuntimeApplication.
  class Delegate {
   public:
    virtual ~Delegate();

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
    using GetAllBindingsCallback =
        base::OnceCallback<void(cast_receiver::Status,
                                std::vector<std::string>)>;
    virtual void GetAllBindings(GetAllBindingsCallback callback) = 0;

    // Creates a new platform-specific MessagePortService.
    virtual std::unique_ptr<MessagePortService> CreateMessagePortService() = 0;

    // Creates a new platform-specific WebUIControllerFactory.
    virtual std::unique_ptr<content::WebUIControllerFactory>
    CreateWebUIControllerFactory(std::vector<std::string> hosts) = 0;
  };

  RuntimeApplication() = default;
  ~RuntimeApplication() override = 0;

  virtual void SetDelegate(Delegate& delegate) = 0;

  // Called before Launch() to perform any pre-launch loading that is
  // necessary. The |callback| will be called indicating if the operation
  // succeeded or not. If Load fails, |this| should be destroyed since it's not
  // necessarily valid to retry Load with a new request.
  virtual void Load(StatusCallback callback) = 0;

  // Called to launch the application. The |callback| will be called indicating
  // if the operation succeeded or not.
  virtual void Launch(StatusCallback callback) = 0;

  // Sets URL rewrite rules.
  virtual void SetUrlRewriteRules(
      url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) = 0;

  // Sets media playback state.
  virtual void SetMediaState(cast::common::MediaState::Type media_state) = 0;

  // Sets visibility state.
  virtual void SetVisibility(cast::common::Visibility::Type visibility) = 0;

  // Sets touch input.
  virtual void SetTouchInput(cast::common::TouchInput::Type touch_input) = 0;

  // Notifies a message port message needs to be handled.
  virtual bool OnMessagePortMessage(cast::web::Message message) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_H_

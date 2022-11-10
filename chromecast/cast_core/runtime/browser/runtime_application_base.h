// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include <string>
#include <vector>
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "components/cast_receiver/browser/public/application_client.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace content {
class WebContents;
class WebUIControllerFactory;
}  // namespace content

namespace chromecast {

class CastWebContents;
class MessagePortService;

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase
    : public cast_receiver::RuntimeApplication,
      public cast_receiver::ContentWindowControls::VisibilityChangeObserver {
 public:
  // This class defines a wrapper around any platform-specific communication
  // details required for functionality of a RuntimeApplicationBase instance.
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

    // Returns the WebContents this application should use.
    // TODO(crbug.com/1382907): Change to a callback-based API.
    virtual content::WebContents* GetWebContents() = 0;

    // Returns the window controls for this instance.
    // TODO(crbug.com/1382907): Change to a callback-based API.
    virtual cast_receiver::ContentWindowControls*
    GetContentWindowControls() = 0;
  };

  ~RuntimeApplicationBase() override;

  void SetDelegate(Delegate& delegate);

  // Called before Launch() to perform any pre-launch loading that is
  // necessary. The |callback| will be called indicating if the operation
  // succeeded or not. If Load fails, |this| should be destroyed since it's not
  // necessarily valid to retry Load with a new request.
  void Load(StatusCallback callback);

  // Called to stop the application. The |callback| will be called indicating
  // if the operation succeeded or not.
  void Stop(StatusCallback callback);

  // Sets URL rewrite rules.
  void SetUrlRewriteRules(
      url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules);

  // Sets media playback state.
  void SetMediaState(cast::common::MediaState::Type media_state);

  // Sets visibility state.
  void SetVisibility(cast::common::Visibility::Type visibility);

  // Sets touch input.
  void SetTouchInput(cast::common::TouchInput::Type touch_input);

  // Returns if current session is enabled for dev.
  //
  // TODO(crbug.com/1359587): Remove this function.
  bool GetEnabledForDev() const;

  // Returns if remote control mode is enabled.
  //
  // TODO(crbug.com/1359587): Remove this function.
  bool GetIsRemoteControlMode() const;

  // Returns the type of Renderer to be used for this application.
  //
  // TODO(crbug.com/1359587): Remove this function.
  mojom::RendererType GetRendererType() const;

  // Called to launch the application. The |callback| will be called indicating
  // if the operation succeeded or not.
  virtual void Launch(StatusCallback callback) = 0;

  // Notifies a message port message needs to be handled.
  virtual bool OnMessagePortMessage(cast::web::Message message) = 0;

  // Partial RuntimeApplication implementation:
  // IsStreamingApplication must be implemented in inherited classes.
  const std::string& GetDisplayName() const override;
  const std::string& GetAppId() const override;
  const std::string& GetCastSessionId() const override;
  bool IsApplicationRunning() const override;

 protected:
  // |application_client| is expected to exist for the lifetime of this
  // instance.
  RuntimeApplicationBase(std::string cast_session_id,
                         cast::common::ApplicationConfig app_config,
                         mojom::RendererType renderer_type,
                         cast_receiver::ApplicationClient& application_client);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication(cast::common::StopReason::Type stop_reason,
                               int32_t net_error_code);

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  Delegate& delegate() { return *delegate_; }

  // NOTE: This field is empty until after Load() is called.
  const cast::common::ApplicationConfig& config() const { return app_config_; }

  // Returns renderer features.
  base::Value GetRendererFeatures() const;

  // Returns if app is audio only.
  bool GetIsAudioOnly() const;

  // Returns if feature permissions are enforced.
  bool GetEnforceFeaturePermissions() const;

  // Returns feature permissions.
  std::vector<int> GetFeaturePermissions() const;

  // Returns additional feature permission origins.
  std::vector<std::string> GetAdditionalFeaturePermissionOrigins() const;

  // Loads the page at the given |url| in the CastWebContents.
  void LoadPage(const GURL& url);

  // Called by the actual implementation as Cast application page has loaded.
  void OnPageLoaded();

 private:
  void SetWebVisibilityAndPaint(bool is_visible);

  // ContentWindowControls::VisibilityChangeObserver implementation:
  void OnWindowShown() override;
  void OnWindowHidden() override;

  // Returns the ApplicationControls associated with this application, if such
  // controls exist.
  // TODO(crbug.com/1382907): Change to a callback-based API.
  cast_receiver::ApplicationClient::ApplicationControls*
  GetApplicationControls();

  const std::string cast_session_id_;
  const cast::common::ApplicationConfig app_config_;

  // Renderer type used by this application.
  mojom::RendererType renderer_type_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::raw_ref<cast_receiver::ApplicationClient> application_client_;

  base::raw_ptr<Delegate> delegate_{nullptr};

  // Cached mojom rules that are set iff |cast_web_view_| is not created before
  // SetUrlRewriteRules is called.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr cached_mojom_rules_{nullptr};

  // Flags whether the application is running or stopped.
  bool is_application_running_ = false;

  cast::common::MediaState::Type media_state_ =
      cast::common::MediaState::LOAD_BLOCKED;
  cast::common::Visibility::Type visibility_ = cast::common::Visibility::HIDDEN;
  cast::common::TouchInput::Type touch_input_ =
      cast::common::TouchInput::DISABLED;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationBase> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include <string>
#include <vector>
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_platform.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cast_core/public/src/proto/common/application_state.pb.h"
#include "third_party/cast_core/public/src/proto/common/value.pb.h"

namespace chromecast {

class CastWebContents;
class CastWebService;

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase : public RuntimeApplication,
                               public CastContentWindow::Observer,
                               public RuntimeApplicationPlatform::Client {
 public:
  ~RuntimeApplicationBase() override;

 protected:
  // |web_service| is expected to exist for the lifetime of this instance.
  RuntimeApplicationBase(
      std::string cast_session_id,
      cast::common::ApplicationConfig app_config,
      mojom::RendererType renderer_type_used,
      CastWebService* web_service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      RuntimeApplicationPlatform::Factory runtime_application_factory);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication(cast::common::StopReason::Type stop_reason,
                               int32_t net_error_code);

  // Called after the application has completed launching.
  virtual void OnApplicationLaunched() = 0;

  // Returns current TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  CastWebContents* cast_web_contents() {
    DCHECK(cast_web_view_);
    return cast_web_view_->cast_web_contents();
  }

  RuntimeApplicationPlatform& application_platform() {
    DCHECK(platform_);
    return *platform_;
  }

  // NOTE: This field is empty until after Load() is called.
  const cast::common::ApplicationConfig& config() const { return app_config_; }

  // RuntimeApplication implementation:
  const std::string& GetDisplayName() const override;
  const std::string& GetAppId() const override;
  const std::string& GetCastSessionId() const override;
  void Load(cast::runtime::LoadApplicationRequest request,
            StatusCallback callback) final;
  void Launch(cast::runtime::LaunchApplicationRequest request,
              StatusCallback callback) final;

  // Returns renderer features.
  base::Value GetRendererFeatures() const;

  // Returns if app is audio only.
  bool GetIsAudioOnly() const;

  // Returns if remote control mode is enabled.
  bool GetIsRemoteControlMode() const;

  // Returns if feature permissions are enforced.
  bool GetEnforceFeaturePermissions() const;

  // Returns feature permissions.
  std::vector<int> GetFeaturePermissions() const;

  // Returns additional feature permission origins.
  std::vector<std::string> GetAdditionalFeaturePermissionOrigins() const;

  // Returns if current session is enabled for dev.
  bool GetEnabledForDev() const;

  // Loads the page at the given |url| in the CastWebContents.
  void LoadPage(const GURL& url);

  // Called by the actual implementation as Cast application page has loaded.
  void OnPageLoaded();

 private:
  // CastContentWindow::Observer implementation:
  void OnVisibilityChange(VisibilityType visibility_type) override;

  // Creates the root CastWebView for this Cast session.
  CastWebView::Scoped CreateCastWebView();

  // PartialRuntimeApplicationPlatform::Client implementation.
  // The following are to be implemented by children of this class:
  // - OnMessagePortMessage(cast::web::Message message)
  void OnUrlRewriteRulesSet(
      url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) override;
  void OnMediaStateSet(cast::common::MediaState::Type media_state) override;
  void OnVisibilitySet(cast::common::Visibility::Type visibility) override;
  void OnTouchInputSet(cast::common::TouchInput::Type touch_input) override;
  bool IsApplicationRunning() override;

  // Calls as RuntimeApplicationPlatform::Load()'s callback.
  void OnApplicationLoading(RuntimeApplication::StatusCallback callback,
                            cast_receiver::Status success);

  // Calls as RuntimeApplicationPlatform::Launch()'s callback.
  void OnApplicationLaunching(RuntimeApplication::StatusCallback callback,
                              cast_receiver::Status success);

  std::unique_ptr<RuntimeApplicationPlatform> platform_;

  const std::string cast_session_id_;
  const cast::common::ApplicationConfig app_config_;

  // The |web_service_| used to create |cast_web_view_|.
  CastWebService* const web_service_;
  // The WebView associated with the window in which the Cast application is
  // displayed.
  CastWebView::Scoped cast_web_view_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Flags whether the application is running or stopped.
  bool is_application_running_ = false;

  // Renderer type used by this application.
  mojom::RendererType renderer_type_;

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

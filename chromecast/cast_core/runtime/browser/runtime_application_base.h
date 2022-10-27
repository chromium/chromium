// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include <string>
#include <vector>
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"

namespace chromecast {

class CastWebContents;
class CastWebService;

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase : public RuntimeApplication,
                               public CastContentWindow::Observer {
 public:
  ~RuntimeApplicationBase() override;

 protected:
  // |web_service| is expected to exist for the lifetime of this instance.
  RuntimeApplicationBase(std::string cast_session_id,
                         cast::common::ApplicationConfig app_config,
                         mojom::RendererType renderer_type_used,
                         CastWebService* web_service);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication(cast::common::StopReason::Type stop_reason,
                               int32_t net_error_code);

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  CastWebContents* cast_web_contents() {
    DCHECK(cast_web_view_);
    return cast_web_view_->cast_web_contents();
  }

  Delegate& delegate() { return *delegate_; }

  // NOTE: This field is empty until after Load() is called.
  const cast::common::ApplicationConfig& config() const { return app_config_; }

  // Partial RuntimeApplication implementation:
  // Launch, OnMessagePortMessage and IsStreamingApplication must be implemented
  // in inherited classes.
  void SetDelegate(Delegate& delegate) override;
  const std::string& GetDisplayName() const override;
  const std::string& GetAppId() const override;
  const std::string& GetCastSessionId() const override;
  void Load(StatusCallback callback) override;
  void Stop(StatusCallback callback) override;
  void SetUrlRewriteRules(
      url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) override;
  void SetMediaState(cast::common::MediaState::Type media_state) override;
  void SetVisibility(cast::common::Visibility::Type visibility) override;
  void SetTouchInput(cast::common::TouchInput::Type touch_input) override;
  bool IsApplicationRunning() const override;

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

  const std::string cast_session_id_;
  const cast::common::ApplicationConfig app_config_;
  // Renderer type used by this application.
  mojom::RendererType renderer_type_;
  // The |web_service_| used to create |cast_web_view_|.
  CastWebService* const web_service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::raw_ptr<Delegate> delegate_{nullptr};
  // Cached mojom rules that are set iff |cast_web_view_| is not created before
  // SetUrlRewriteRules is called.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr cached_mojom_rules_{nullptr};

  // The WebView associated with the window in which the Cast application is
  // displayed.
  CastWebView::Scoped cast_web_view_;

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

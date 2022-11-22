// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/cast_receiver/browser/public/application_client.h"
#include "components/cast_receiver/browser/public/application_config.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

class CastWebContents;

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase
    : public cast_receiver::RuntimeApplication,
      public cast_receiver::ContentWindowControls::VisibilityChangeObserver {
 public:
  ~RuntimeApplicationBase() override;

  // Sets the |embedder_application| to be used for making calls to platform-
  // specific implementations of cast_receiver interfaces.
  void SetEmbedderApplication(
      cast_receiver::EmbedderApplication& embedder_application);

  // RuntimeApplication implementation.
  //
  // To be implemented by descendants of this class:
  // - Launch(StatusCallback callback)
  // - IsStreamingApplication()
  void Load(StatusCallback callback) override;
  void Stop(StatusCallback callback) override;
  void SetUrlRewriteRules(
      url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) override;
  void SetMediaBlocking(bool load_blocked, bool start_blocked) override;
  void SetVisibility(bool is_visible) override;
  void SetTouchInputEnabled(bool enabled) override;
  const std::string& GetDisplayName() const override;
  const std::string& GetAppId() const override;
  const std::string& GetCastSessionId() const override;
  bool IsApplicationRunning() const override;

 protected:
  // |application_client| is expected to exist for the lifetime of this
  // instance.
  RuntimeApplicationBase(std::string cast_session_id,
                         cast_receiver::ApplicationConfig app_config,
                         cast_receiver::ApplicationClient& application_client);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication(
      cast_receiver::EmbedderApplication::ApplicationStopReason stop_reason,
      int32_t net_error_code);

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  cast_receiver::EmbedderApplication& embedder_application() {
    DCHECK(embedder_application_);
    return *embedder_application_;
  }

  // NOTE: This field is empty until after Load() is called.
  const cast_receiver::ApplicationConfig& config() const { return app_config_; }

  // Loads the page at the given |url| in the CastWebContents.
  void LoadPage(const GURL& url);

  // Called by the actual implementation as Cast application page has loaded.
  void OnPageLoaded();

  // Sets the permissions for the provided |web_contents| to that as configured
  // in |app_config_|.
  void SetContentPermissions(content::WebContents& web_contents);

  // Returns the ApplicationControls associated with this application, if such
  // controls exist.
  // TODO(crbug.com/1382907): Change to a callback-based API.
  cast_receiver::ApplicationClient::ApplicationControls&
  GetApplicationControls();

 private:
  void SetWebVisibilityAndPaint(bool is_visible);

  // ContentWindowControls::VisibilityChangeObserver implementation:
  void OnWindowShown() override;
  void OnWindowHidden() override;

  const std::string cast_session_id_;
  const cast_receiver::ApplicationConfig app_config_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::raw_ref<cast_receiver::ApplicationClient> application_client_;

  base::raw_ptr<cast_receiver::EmbedderApplication> embedder_application_{
      nullptr};

  // Cached mojom rules that are set iff |cast_web_view_| is not created before
  // SetUrlRewriteRules is called.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr cached_mojom_rules_{nullptr};

  // Flags whether the application is running or stopped.
  bool is_application_running_ = false;

  // Media-related states of the application.
  bool is_media_load_blocked_ = true;
  bool is_media_start_blocked_ = true;
  bool is_visible_ = false;
  bool is_touch_input_enabled_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationBase> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_BASE_H_

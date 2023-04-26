// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_RUNTIME_APPLICATION_BASE_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_RUNTIME_APPLICATION_BASE_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/cast_receiver/browser/application_client.h"
#include "components/cast_receiver/browser/public/application_config.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "net/base/net_errors.h"

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

// This class is for sharing code between Web and streaming RuntimeApplication
// implementations, including Load and Launch behavior.
class RuntimeApplicationBase
    : public RuntimeApplication,
      public ContentWindowControls::VisibilityChangeObserver {
 public:
  ~RuntimeApplicationBase() override;

  RuntimeApplicationBase(RuntimeApplicationBase& other) = delete;
  RuntimeApplicationBase& operator=(RuntimeApplicationBase& other) = delete;

  // Sets the |embedder_application| to be used for making calls to platform-
  // specific implementations of cast_receiver interfaces.
  void SetEmbedderApplication(EmbedderApplication& embedder_application);

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
                         ApplicationConfig app_config,
                         ApplicationClient& application_client);

  // Stops the running application. Must be called before destruction of any
  // instance of the implementing object.
  virtual void StopApplication(
      EmbedderApplication::ApplicationStopReason stop_reason,
      net::Error net_error_code);

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  EmbedderApplication& embedder_application() {
    DCHECK(embedder_application_);
    return *embedder_application_;
  }

  // NOTE: This field is empty until after Load() is called.
  const ApplicationConfig& config() const { return app_config_; }

  ApplicationClient& application_client() { return *application_client_; }

  // Navigated to the page at the given |url| in the associated WebContents.
  void NavigateToPage(const GURL& url);

  // Called by the actual implementation after the Cast application page has
  // been navigated to, following a call to NavigateToPage().
  void OnPageNavigationComplete();

  // Sets the permissions for the provided |web_contents| to that as configured
  // in |app_config_|.
  void SetContentPermissions(content::WebContents& web_contents);

  // Returns the ApplicationControls associated with this application, if such
  // controls exist.
  //
  // TODO(crbug.com/1382907): Change to a callback-based API.
  ApplicationClient::ApplicationControls& GetApplicationControls();

 private:
  void SetWebVisibilityAndPaint(bool is_visible);

  // ContentWindowControls::VisibilityChangeObserver implementation:
  void OnWindowShown() override;
  void OnWindowHidden() override;

  const std::string cast_session_id_;
  const ApplicationConfig app_config_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ref<ApplicationClient> application_client_;

  raw_ptr<EmbedderApplication> embedder_application_{nullptr};

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

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_RUNTIME_APPLICATION_BASE_H_

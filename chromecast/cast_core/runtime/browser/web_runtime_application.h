// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

#include <vector>

#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "components/cast_receiver/browser/page_state_observer.h"
#include "components/cast_receiver/browser/public/application_config.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromecast {

class BindingsManagerWebRuntime;

class WebRuntimeApplication final : public RuntimeApplicationBase,
                                    public content::WebContentsObserver,
                                    public BindingsManagerWebRuntime::Client,
                                    public cast_receiver::PageStateObserver {
 public:
  // |web_service| is expected to exist for the lifetime of this instance.
  WebRuntimeApplication(std::string cast_session_id,
                        cast_receiver::ApplicationConfig app_config,
                        cast_receiver::ApplicationClient& application_client);
  ~WebRuntimeApplication() override;

 private:
  const GURL& app_url() {
    DCHECK(config().url);
    return *config().url;
  }

  void OnAllBindingsReceived(cast_receiver::Status status,
                             std::vector<std::string> bindings);

  // RuntimeApplicationBase implementation:
  void Launch(StatusCallback callback) override;
  bool IsStreamingApplication() const override;

  // cast_receiver::PageStateObserver implementation:
  void OnPageLoadComplete() override;
  void OnPageStopped(StopReason reason, int32_t error_code) override;

  // content::WebContentsObserver implementation:
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override;

  // BindingsManagerWebRuntime::Client implementation:
  void OnError() override;

  std::unique_ptr<BindingsManagerWebRuntime> bindings_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WebRuntimeApplication> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

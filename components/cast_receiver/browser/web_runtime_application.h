// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_WEB_RUNTIME_APPLICATION_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_WEB_RUNTIME_APPLICATION_H_

#include <vector>

#include "components/cast_receiver/browser/bindings_manager.h"
#include "components/cast_receiver/browser/page_state_observer.h"
#include "components/cast_receiver/browser/public/application_config.h"
#include "components/cast_receiver/browser/runtime_application_base.h"
#include "content/public/browser/web_contents_observer.h"

namespace cast_receiver {

class ApplicationClient;

class WebRuntimeApplication final : public RuntimeApplicationBase,
                                    public content::WebContentsObserver,
                                    public BindingsManager::Client,
                                    public PageStateObserver {
 public:
  // |web_service| is expected to exist for the lifetime of this instance.
  WebRuntimeApplication(std::string cast_session_id,
                        ApplicationConfig app_config,
                        ApplicationClient& application_client);
  ~WebRuntimeApplication() override;

  WebRuntimeApplication(WebRuntimeApplication& other) = delete;
  WebRuntimeApplication& operator=(WebRuntimeApplication& other) = delete;

 private:
  const GURL& app_url() {
    DCHECK(config().url);
    return *config().url;
  }

  void OnAllBindingsReceived(Status status, std::vector<std::string> bindings);

  // RuntimeApplicationBase implementation:
  void Launch(StatusCallback callback) override;
  bool IsStreamingApplication() const override;

  // PageStateObserver implementation:
  void OnPageLoadComplete() override;
  void OnPageStopped(StopReason reason, net::Error error_code) override;

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

  std::unique_ptr<BindingsManager> bindings_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WebRuntimeApplication> weak_factory_{this};
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_WEB_RUNTIME_APPLICATION_H_

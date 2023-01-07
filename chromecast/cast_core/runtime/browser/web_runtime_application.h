// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_platform.h"
#include "components/cast_receiver/browser/page_state_observer.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromecast {

class BindingsManagerWebRuntime;
class CastWebService;

class WebRuntimeApplication final : public RuntimeApplicationBase,
                                    public content::WebContentsObserver,
                                    public cast_receiver::PageStateObserver {
 public:
  // |web_service| is expected to exist for the lifetime of this instance.
  WebRuntimeApplication(
      std::string cast_session_id,
      cast::common::ApplicationConfig app_config,
      CastWebService* web_service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      RuntimeApplicationPlatform::Factory runtime_application_factory);
  ~WebRuntimeApplication() override;

 private:
  void OnAllBindingsReceived(
      absl::optional<cast::bindings::GetAllResponse> response);

  // RuntimeApplicationBase implementation:
  bool OnMessagePortMessage(cast::web::Message message) override;
  void OnApplicationLaunched() override;
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

  const GURL app_url_;
  std::unique_ptr<BindingsManagerWebRuntime> bindings_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WebRuntimeApplication> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

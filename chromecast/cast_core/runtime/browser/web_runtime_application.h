// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "content/public/browser/web_contents_observer.h"

namespace chromecast {

class BindingsManagerWebRuntime;
class CastWebService;

class WebRuntimeApplication final : public RuntimeApplicationBase,
                                    public content::WebContentsObserver {
 public:
  // |web_service| is expected to exist for the lifetime of this instance.
  WebRuntimeApplication(std::string cast_session_id,
                        cast::common::ApplicationConfig app_config,
                        CastWebService* web_service,
                        scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebRuntimeApplication() override;

 private:
  // RuntimeApplicationBase implementation:
  cast::utils::GrpcStatusOr<cast::web::MessagePortStatus> HandlePortMessage(
      cast::web::Message message) override;
  void LaunchApplication() override;
  bool IsStreamingApplication() const override;

  // content::WebContentsObserver implementation:
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void WebContentsDestroyed() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  void OnAllBindingsReceived(
      cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or);

  const GURL app_url_;
  std::unique_ptr<BindingsManagerWebRuntime> bindings_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WebRuntimeApplication> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_WEB_RUNTIME_APPLICATION_H_

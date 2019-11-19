// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_media_info_manager.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_renderer_block_data.h"
#include "content/public/browser/web_contents.h"

#include <utility>

namespace chromecast {
namespace media {

void CreateApplicationMediaInfoManager(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    bool mixer_audio_enabled,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver) {
  // The created ApplicationMediaInfoManager will be deleted on connection
  // error, or when the frame navigates away. See FrameServiceBase for details.
  new ApplicationMediaInfoManager(render_frame_host, std::move(receiver),
                                  std::move(application_session_id),
                                  mixer_audio_enabled);
}

ApplicationMediaInfoManager::ApplicationMediaInfoManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver,
    std::string application_session_id,
    bool mixer_audio_enabled)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      application_session_id_(std::move(application_session_id)),
      mixer_audio_enabled_(mixer_audio_enabled),
      renderer_blocked_(false) {
  shell::CastRendererBlockData::SetApplicationMediaInfoManagerForWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host), this);
}

ApplicationMediaInfoManager::~ApplicationMediaInfoManager() = default;

void ApplicationMediaInfoManager::SetRendererBlock(bool renderer_blocked) {
  LOG(INFO) << "Setting blocked to: " << renderer_blocked << " from "
            << renderer_blocked_
            << "(Pending call set: " << (!pending_call_.is_null()) << ")";
  if (renderer_blocked_ && !renderer_blocked && pending_call_) {
    // Move callbacks in case CanStartRenderer() is called.
    std::move(pending_call_)
        .Run(::media::mojom::CastApplicationMediaInfo::New(
            application_session_id_, mixer_audio_enabled_));
    pending_call_.Reset();
  }

  renderer_blocked_ = renderer_blocked;
}

void ApplicationMediaInfoManager::GetCastApplicationMediaInfo(
    GetCastApplicationMediaInfoCallback callback) {
  LOG(INFO) << "GetCastApplicationMediaInfo called with blocked: "
            << renderer_blocked_;

  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
      "Cast.Platform.CastRenderer.MediaReady", renderer_blocked_);

  if (renderer_blocked_) {
    DCHECK(!pending_call_);
    pending_call_ = std::move(callback);
    return;
  }

  std::move(callback).Run(::media::mojom::CastApplicationMediaInfo::New(
      application_session_id_, mixer_audio_enabled_));
}

}  // namespace media
}  // namespace chromecast

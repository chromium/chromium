// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_media_info_manager.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_renderer_block_data.h"
#include "chromecast/browser/cast_session_id_map.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {
namespace media {
namespace {
void RunGetCastApplicationMediaInfoCallback(
    ApplicationMediaInfoManager::GetCastApplicationMediaInfoCallback callback,
    const std::string& session_id,
    bool mixer_audio_enabled,
    bool is_audio_only_session) {
  std::move(callback).Run(::media::mojom::CastApplicationMediaInfo::New(
      session_id, mixer_audio_enabled, is_audio_only_session));
}
}  // namespace

void ApplicationMediaInfoManager::Create(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    bool mixer_audio_enabled,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver) {
  CHECK(render_frame_host);
  // The created ApplicationMediaInfoManager will be deleted on connection
  // error, or when the frame navigates away. See DocumentService for
  // details.
  new ApplicationMediaInfoManager(*render_frame_host, std::move(receiver),
                                  std::move(application_session_id),
                                  mixer_audio_enabled);
}

ApplicationMediaInfoManager& ApplicationMediaInfoManager::CreateForTesting(
    content::RenderFrameHost& render_frame_host,
    std::string application_session_id,
    bool mixer_audio_enabled,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver) {
  return *new ApplicationMediaInfoManager(
      render_frame_host, std::move(receiver), std::move(application_session_id),
      mixer_audio_enabled);
}

ApplicationMediaInfoManager::ApplicationMediaInfoManager(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver,
    std::string application_session_id,
    bool mixer_audio_enabled)
    : DocumentService(render_frame_host, std::move(receiver)),
      application_session_id_(std::move(application_session_id)),
      mixer_audio_enabled_(mixer_audio_enabled),
      renderer_blocked_(false) {
  shell::CastRendererBlockData::SetApplicationMediaInfoManagerForWebContents(
      content::WebContents::FromRenderFrameHost(&render_frame_host),
      weak_ptr_factory_.GetWeakPtr());
}

ApplicationMediaInfoManager::~ApplicationMediaInfoManager() = default;

void ApplicationMediaInfoManager::SetRendererBlock(bool renderer_blocked) {
  LOG(INFO) << "Setting blocked to: " << renderer_blocked << " from "
            << renderer_blocked_
            << "(Pending call set: " << (!pending_call_.is_null()) << ")";
  if (renderer_blocked_ && !renderer_blocked && pending_call_) {
    shell::CastSessionIdMap::GetInstance()->IsAudioOnlySessionAsync(
        application_session_id_,
        base::BindOnce(&RunGetCastApplicationMediaInfoCallback,
                       // Move callbacks in case CanStartRenderer() is called.
                       std::move(pending_call_), application_session_id_,
                       mixer_audio_enabled_));
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

  shell::CastSessionIdMap::GetInstance()->IsAudioOnlySessionAsync(
      application_session_id_,
      base::BindOnce(&RunGetCastApplicationMediaInfoCallback,
                     std::move(callback), application_session_id_,
                     mixer_audio_enabled_));
}

}  // namespace media
}  // namespace chromecast

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_
#define CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/frame_service_base.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chromecast {
namespace media {

class ApplicationMediaInfoManagerTest;

class ApplicationMediaInfoManager
    : public ::content::FrameServiceBase<
          ::media::mojom::CastApplicationMediaInfoManager>,
      public base::SupportsWeakPtr<ApplicationMediaInfoManager> {
 public:
  ApplicationMediaInfoManager(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
          receiver,
      std::string application_session_id,
      bool mixer_audio_enabled);
  ~ApplicationMediaInfoManager() override;

  void SetRendererBlock(bool renderer_blocked);

 private:
  friend ApplicationMediaInfoManagerTest;
  // ::media::mojom::CastApplicationMediaInfoManager implementation:
  void GetCastApplicationMediaInfo(
      GetCastApplicationMediaInfoCallback callback) final;

  GetCastApplicationMediaInfoCallback pending_call_;
  const std::string application_session_id_;
  bool mixer_audio_enabled_;
  // Flag to determine if renderer can start.
  bool renderer_blocked_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationMediaInfoManager);
};

void CreateApplicationMediaInfoManager(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    bool mixer_audio_enabled,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_

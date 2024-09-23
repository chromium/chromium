// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_
#define CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_service.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chromecast {
namespace media {

class ApplicationMediaInfoManagerTest;

class ApplicationMediaInfoManager final
    : public ::content::DocumentService<
          ::media::mojom::CastApplicationMediaInfoManager> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      std::string application_session_id,
      bool mixer_audio_enabled,
      mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
          receiver);
  static ApplicationMediaInfoManager& CreateForTesting(
      content::RenderFrameHost& render_frame_host,
      std::string application_session_id,
      bool mixer_audio_enabled,
      mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
          receiver);

  ApplicationMediaInfoManager(const ApplicationMediaInfoManager&) = delete;
  ApplicationMediaInfoManager& operator=(const ApplicationMediaInfoManager&) =
      delete;

  void SetRendererBlock(bool renderer_blocked);

 private:
  friend ApplicationMediaInfoManagerTest;

  ApplicationMediaInfoManager(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
          receiver,
      std::string application_session_id,
      bool mixer_audio_enabled);
  ~ApplicationMediaInfoManager() override;

  // ::media::mojom::CastApplicationMediaInfoManager implementation:
  void GetCastApplicationMediaInfo(
      GetCastApplicationMediaInfoCallback callback) final;

  GetCastApplicationMediaInfoCallback pending_call_;
  const std::string application_session_id_;
  bool mixer_audio_enabled_;
  // Flag to determine if renderer can start.
  bool renderer_blocked_;
  base::WeakPtrFactory<ApplicationMediaInfoManager> weak_ptr_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_

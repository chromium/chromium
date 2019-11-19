// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"

#include "base/allocator/buildflags.h"
#include "base/bind.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/service_process_host.h"
#include "media/media_buildflags.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_FFMPEG)
#include "third_party/ffmpeg/ffmpeg_features.h"  // nogncheck
extern "C" {
#include <libavutil/cpu.h>
}
#endif

MediaParserProvider::MediaParserProvider() = default;

MediaParserProvider::~MediaParserProvider() = default;

void MediaParserProvider::RetrieveMediaParser() {
  DCHECK(!remote_media_parser_factory_);
  DCHECK(!remote_media_parser_);

  content::ServiceProcessHost::Launch(
      remote_media_parser_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_MEDIA_GALLERY_UTILITY_NAME)
          .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
          .Pass());
  remote_media_parser_factory_.set_disconnect_handler(base::BindOnce(
      &MediaParserProvider::OnConnectionError, base::Unretained(this)));

  int libyuv_cpu_flags = libyuv::InitCpuFlags();

#if BUILDFLAG(ENABLE_FFMPEG)
  int avutil_cpu_flags = av_get_cpu_flags();
#else
  int avutil_cpu_flags = -1;
#endif

  remote_media_parser_factory_->CreateMediaParser(
      libyuv_cpu_flags, avutil_cpu_flags,
      base::BindOnce(&MediaParserProvider::OnMediaParserCreatedImpl,
                     base::Unretained(this)));
}

void MediaParserProvider::OnMediaParserCreatedImpl(
    mojo::PendingRemote<chrome::mojom::MediaParser> remote_media_parser) {
  remote_media_parser_.Bind(std::move(remote_media_parser));
  remote_media_parser_.set_disconnect_handler(base::BindOnce(
      &MediaParserProvider::OnConnectionError, base::Unretained(this)));

  OnMediaParserCreated();
}

void MediaParserProvider::ResetMediaParser() {
  remote_media_parser_.reset();
  remote_media_parser_factory_.reset();
}

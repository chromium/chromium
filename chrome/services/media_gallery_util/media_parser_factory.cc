// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser_factory.h"

#include "build/build_config.h"
#include "chrome/services/media_gallery_util/media_parser.h"
#include "media/base/media.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/services/media_gallery_util/media_parser_android.h"
#endif

MediaParserFactory::MediaParserFactory(
    mojo::PendingReceiver<chrome::mojom::MediaParserFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

MediaParserFactory::~MediaParserFactory() = default;

void MediaParserFactory::CreateMediaParser(int64_t libyuv_cpu_flags,
                                           int64_t libavutil_cpu_flags,
                                           CreateMediaParserCallback callback) {
  media::InitializeMediaLibraryInSandbox(libyuv_cpu_flags, libavutil_cpu_flags);

  mojo::PendingRemote<chrome::mojom::MediaParser> remote_media_parser;
  std::unique_ptr<MediaParser> media_parser;
#if BUILDFLAG(IS_ANDROID)
  media_parser = std::make_unique<MediaParserAndroid>();
#else
  media_parser = std::make_unique<MediaParser>();
#endif
  mojo::MakeSelfOwnedReceiver(
      std::move(media_parser),
      remote_media_parser.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(remote_media_parser));
}

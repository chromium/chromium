// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser.h"

#include "base/bind.h"
#include "chrome/services/media_gallery_util/ipc_data_source.h"
#include "chrome/services/media_gallery_util/media_metadata_parser.h"
#include "media/media_buildflags.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/media_file_checker.h"
extern "C" {
#include <libavutil/cpu.h>
}
#endif

namespace {

void ParseMediaMetadataDone(
    MediaParser::ParseMediaMetadataCallback callback,
    MediaMetadataParser* /* parser */,
    chrome::mojom::MediaMetadataPtr metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  std::move(callback).Run(true, std::move(metadata), attached_images);
}

}  // namespace

MediaParser::MediaParser() = default;

MediaParser::~MediaParser() = default;

void MediaParser::ParseMediaMetadata(
    const std::string& mime_type,
    int64_t total_size,
    bool get_attached_images,
    mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
    ParseMediaMetadataCallback callback) {
  auto source =
      std::make_unique<IPCDataSource>(std::move(media_data_source), total_size);
  MediaMetadataParser* parser = new MediaMetadataParser(
      std::move(source), mime_type, get_attached_images);
  parser->Start(base::Bind(&ParseMediaMetadataDone, base::Passed(&callback),
                           base::Owned(parser)));
}

void MediaParser::CheckMediaFile(base::TimeDelta decode_time,
                                 base::File file,
                                 CheckMediaFileCallback callback) {
#if BUILDFLAG(ENABLE_FFMPEG)
  media::MediaFileChecker checker(std::move(file));
  std::move(callback).Run(checker.Start(decode_time));
#else
  std::move(callback).Run(false);
#endif
}

void MediaParser::GetCpuInfo(GetCpuInfoCallback callback) {
  int64_t ffmpeg_cpu_flags = 0;
#if BUILDFLAG(ENABLE_FFMPEG)
  ffmpeg_cpu_flags = av_get_cpu_flags();
#endif
  std::move(callback).Run(libyuv::InitCpuFlags(), ffmpeg_cpu_flags);
}

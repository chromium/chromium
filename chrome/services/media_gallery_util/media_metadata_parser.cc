// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_metadata_parser.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "media/base/data_source.h"
#include "media/filters/audio_video_metadata_extractor.h"
#include "media/media_buildflags.h"
#include "net/base/mime_sniffer.h"

namespace {

// This runs on |media_thread_|, as the underlying FFmpeg operation is
// blocking, and the utility thread must not be blocked, so the media file
// bytes can be sent from the browser process to the utility process.
chrome::mojom::MediaMetadataPtr ParseAudioVideoMetadata(
    media::DataSource* source,
    bool get_attached_images,
    const std::string& mime_type,
    std::vector<metadata::AttachedImage>* attached_images) {
  DCHECK(source);

  chrome::mojom::MediaMetadataPtr metadata =
      chrome::mojom::MediaMetadata::New();
  metadata->mime_type = mime_type;

#if BUILDFLAG(ENABLE_FFMPEG)
  media::AudioVideoMetadataExtractor extractor;

  if (!extractor.Extract(source, get_attached_images))
    return metadata;

  if (extractor.has_duration() && extractor.duration() >= 0)
    metadata->duration = extractor.duration();

  if (extractor.height() >= 0 && extractor.width() >= 0) {
    metadata->height = extractor.height();
    metadata->width = extractor.width();
  }

  metadata->artist = extractor.artist();
  metadata->album = extractor.album();
  metadata->comment = extractor.comment();
  metadata->copyright = extractor.copyright();
  metadata->disc = extractor.disc();
  metadata->genre = extractor.genre();
  metadata->language = extractor.language();
  metadata->rotation = extractor.rotation();
  metadata->title = extractor.title();
  metadata->track = extractor.track();

  for (const auto& it : extractor.stream_infos()) {
    chrome::mojom::MediaStreamInfoPtr stream_info =
        chrome::mojom::MediaStreamInfo::New(it.type, base::Value::Dict());
    for (const auto& tag : it.tags) {
      stream_info->additional_properties.Set(tag.first, tag.second);
    }
    metadata->raw_tags.push_back(std::move(stream_info));
  }

  if (get_attached_images) {
    for (auto it = extractor.attached_images_bytes().begin();
         it != extractor.attached_images_bytes().end(); ++it) {
      attached_images->push_back(metadata::AttachedImage());
      attached_images->back().data = *it;
      net::SniffMimeTypeFromLocalData(*it, &attached_images->back().type);
    }
  }
#endif
  return metadata;
}

void FinishParseAudioVideoMetadata(
    MediaMetadataParser::MetadataCallback callback,
    std::vector<metadata::AttachedImage>* attached_images,
    chrome::mojom::MediaMetadataPtr metadata) {
  DCHECK(!callback.is_null());
  DCHECK(metadata);
  DCHECK(attached_images);

  std::move(callback).Run(std::move(metadata), *attached_images);
}

bool IsSupportedMetadataMimetype(const std::string& mime_type) {
  if (base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE))
    return true;
  if (base::StartsWith(mime_type, "video/", base::CompareCase::SENSITIVE))
    return true;
  return false;
}

}  // namespace

MediaMetadataParser::MediaMetadataParser(
    std::unique_ptr<media::DataSource> source,
    const std::string& mime_type,
    bool get_attached_images)
    : source_(std::move(source)),
      mime_type_(mime_type),
      get_attached_images_(get_attached_images) {}

MediaMetadataParser::~MediaMetadataParser() = default;

void MediaMetadataParser::Start(MetadataCallback callback) {
  if (!IsSupportedMetadataMimetype(mime_type_)) {
    std::move(callback).Run(chrome::mojom::MediaMetadata::New(),
                            std::vector<metadata::AttachedImage>());
    return;
  }

  auto* images = new std::vector<metadata::AttachedImage>();

  media_thread_ = std::make_unique<base::Thread>("media_thread");
  CHECK(media_thread_->Start());

  media_thread_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ParseAudioVideoMetadata, source_.get(),
                     get_attached_images_, mime_type_, images),
      base::BindOnce(&FinishParseAudioVideoMetadata, std::move(callback),
                     base::Owned(images)));
}

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/cpp/safe_media_metadata_parser.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

SafeMediaMetadataParser::SafeMediaMetadataParser(
    int64_t size,
    const std::string& mime_type,
    bool get_attached_images,
    std::unique_ptr<MediaDataSourceFactory> media_source_factory)
    : size_(size),
      mime_type_(mime_type),
      get_attached_images_(get_attached_images),
      media_source_factory_(std::move(media_source_factory)) {}

SafeMediaMetadataParser::~SafeMediaMetadataParser() = default;

void SafeMediaMetadataParser::Start(DoneCallback callback) {
  DCHECK(!media_parser());
  DCHECK(callback);

  callback_ = std::move(callback);

  RetrieveMediaParser();
}

void SafeMediaMetadataParser::OnMediaParserCreated() {
  mojo::PendingRemote<chrome::mojom::MediaDataSource> source;
  media_data_source_ = media_source_factory_->CreateMediaDataSource(
      source.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&SafeMediaMetadataParser::OnMediaDataReady,
                          weak_factory_.GetWeakPtr()));
  media_parser()->ParseMediaMetadata(
      mime_type_, size_, get_attached_images_, std::move(source),
      base::BindOnce(&SafeMediaMetadataParser::ParseMediaMetadataDone,
                     base::Unretained(this)));
}

void SafeMediaMetadataParser::OnConnectionError() {
  chrome::mojom::MediaMetadataPtr metadata =
      chrome::mojom::MediaMetadata::New();
  auto attached_images =
      std::make_unique<std::vector<metadata::AttachedImage>>();

  std::move(callback_).Run(/*parse_success=*/false, std::move(metadata),
                           std::move(attached_images));
}

void SafeMediaMetadataParser::ParseMediaMetadataDone(
    bool parse_success,
    chrome::mojom::MediaMetadataPtr metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  ResetMediaParser();
  media_data_source_.reset();

  auto attached_images_copy =
      std::make_unique<std::vector<metadata::AttachedImage>>(attached_images);

  std::move(callback_).Run(parse_success, std::move(metadata),
                           std::move(attached_images_copy));
}

void SafeMediaMetadataParser::OnMediaDataReady(
    chrome::mojom::MediaDataSource::ReadCallback callback,
    std::string data) {
  if (media_parser())
    std::move(callback).Run(std::vector<uint8_t>(data.begin(), data.end()));
}

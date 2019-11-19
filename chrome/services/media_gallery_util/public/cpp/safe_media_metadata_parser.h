// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_MEDIA_METADATA_PARSER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_MEDIA_METADATA_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// Parses the media metadata safely in a utility process. This class expects the
// MIME type and the size of media data to be already known. It creates a
// utility process to do further MIME-type-specific metadata extraction from the
// media data.
class SafeMediaMetadataParser : public MediaParserProvider {
 public:
  typedef base::OnceCallback<void(
      bool parse_success,
      chrome::mojom::MediaMetadataPtr metadata,
      std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images)>
      DoneCallback;

  // Factory to create media data source instance. The underlying implementation
  // may read from different kinds of storage.
  class MediaDataSourceFactory {
   public:
    typedef base::RepeatingCallback<void(
        chrome::mojom::MediaDataSource::ReadCallback callback,
        std::unique_ptr<std::string> data)>
        MediaDataCallback;

    virtual std::unique_ptr<chrome::mojom::MediaDataSource>
    CreateMediaDataSource(
        mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
        MediaDataCallback media_data_callback) = 0;
    virtual ~MediaDataSourceFactory() {}
  };

  SafeMediaMetadataParser(
      int64_t size,
      const std::string& mime_type,
      bool get_attached_images,
      std::unique_ptr<MediaDataSourceFactory> media_source_factory);
  ~SafeMediaMetadataParser() override;

  // Initiates parsing. |callback| is invoked on the same sequence that calls
  // this method.
  void Start(DoneCallback callback);

 private:
  // MediaParserProvider implementation:
  void OnMediaParserCreated() override;
  void OnConnectionError() override;

  // Callback from utility process when it finishes parsing metadata.
  void ParseMediaMetadataDone(
      bool parse_success,
      chrome::mojom::MediaMetadataPtr metadata,
      const std::vector<metadata::AttachedImage>& attached_images);

  // Invoked when the media data has been read, which will be sent back to
  // utility process soon. |data| might be partial content of the media data.
  void OnMediaDataReady(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::unique_ptr<std::string> data);

  const int64_t size_;
  const std::string mime_type_;
  bool get_attached_images_;

  DoneCallback callback_;

  std::unique_ptr<chrome::mojom::MediaDataSource> media_data_source_;
  std::unique_ptr<MediaDataSourceFactory> media_source_factory_;

  base::WeakPtrFactory<SafeMediaMetadataParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SafeMediaMetadataParser);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_MEDIA_METADATA_PARSER_H_

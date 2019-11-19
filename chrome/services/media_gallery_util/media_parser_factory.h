// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_FACTORY_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class MediaParserFactory : public chrome::mojom::MediaParserFactory {
 public:
  explicit MediaParserFactory(
      mojo::PendingReceiver<chrome::mojom::MediaParserFactory> receiver);
  ~MediaParserFactory() override;

 private:
  // chrome::mojom::MediaParserFactory:
  void CreateMediaParser(int64_t libyuv_cpu_flags,
                         int64_t ffmpeg_cpu_flags,
                         CreateMediaParserCallback callback) override;

  mojo::Receiver<chrome::mojom::MediaParserFactory> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MediaParserFactory);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_FACTORY_H_

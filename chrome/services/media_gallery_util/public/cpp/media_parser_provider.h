// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_MEDIA_PARSER_PROVIDER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_MEDIA_PARSER_PROVIDER_H_

#include "base/macros.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// Base class used by SafeMediaMetadataParser and SafeAudioVideoChecker to
// retrieve a mojo::Remote<MediaParser>.
class MediaParserProvider {
 public:
  MediaParserProvider();
  virtual ~MediaParserProvider();

 protected:
  // Acquires the remote MediaParser interface asynchronously from a new
  // service process. |OnMediaParserCreated()| is called if and when the media
  // parser is available.
  void RetrieveMediaParser();

  // Invoked when the media parser was successfully created. It can then be
  // obtained by calling media_parser() which is then guaranteed to be bound.
  virtual void OnMediaParserCreated() = 0;

  // Invoked when there was an error with the connection to the media gallerie
  // util service. When this call happens, it means any pending callback
  // expected from media_parser() will not happen.
  virtual void OnConnectionError() = 0;

  const mojo::Remote<chrome::mojom::MediaParser>& media_parser() const {
    return remote_media_parser_;
  }

  // Clears all remote handles to the media gallery service process.
  void ResetMediaParser();

 private:
  void OnMediaParserCreatedImpl(
      mojo::PendingRemote<chrome::mojom::MediaParser> remote_media_parser);

  mojo::Remote<chrome::mojom::MediaParserFactory> remote_media_parser_factory_;
  mojo::Remote<chrome::mojom::MediaParser> remote_media_parser_;

  DISALLOW_COPY_AND_ASSIGN(MediaParserProvider);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_MEDIA_PARSER_PROVIDER_H_

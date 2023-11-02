// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_SUPPORTED_CODEC_FINDER_H_
#define CHROMECAST_BROWSER_MEDIA_SUPPORTED_CODEC_FINDER_H_

namespace chromecast {
namespace media {

class MediaCapsImpl;

class SupportedCodecFinder {
 public:
  // Notifies the given MediaCaps of all found supported codecs.
  void FindSupportedCodecProfileLevels(MediaCapsImpl* media_caps);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_SUPPORTED_CODEC_FINDER_H_

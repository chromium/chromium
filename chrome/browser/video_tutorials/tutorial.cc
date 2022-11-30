// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/tutorial.h"

namespace video_tutorials {

Tutorial::Tutorial() : feature(FeatureType::kInvalid), video_length(0) {}

Tutorial::Tutorial(FeatureType feature,
                   const std::string& title,
                   const std::string& video_url,
                   const std::string& share_url,
                   const std::string& poster_url,
                   const std::string& animated_gif_url,
                   const std::string& thumbnail_url,
                   const std::string& caption_url,
                   int video_length)
    : feature(feature),
      title(title),
      video_url(video_url),
      share_url(share_url),
      poster_url(poster_url),
      animated_gif_url(animated_gif_url),
      thumbnail_url(thumbnail_url),
      caption_url(caption_url),
      video_length(video_length) {}

bool Tutorial::operator==(const Tutorial& other) const {
  return feature == other.feature && title == other.title &&
         video_url == other.video_url && share_url == other.share_url &&
         poster_url == other.poster_url &&
         animated_gif_url == other.animated_gif_url &&
         thumbnail_url == other.thumbnail_url &&
         caption_url == other.caption_url && video_length == other.video_length;
}

bool Tutorial::operator!=(const Tutorial& other) const {
  return !(*this == other);
}

Tutorial::~Tutorial() = default;

Tutorial::Tutorial(const Tutorial& other) = default;

Tutorial& Tutorial::operator=(const Tutorial& other) = default;

}  // namespace video_tutorials

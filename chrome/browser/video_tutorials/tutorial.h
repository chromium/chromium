// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_H_

#include <string>
#include "base/logging.h"
#include "url/gurl.h"

namespace video_tutorials {

// Please align this enum with
// chrome/browser/video_tutorials/proto/video_tutorials.proto and variants
// Feature in
// tools/metrics/histograms/metadata/video_tutorials/histograms.xml.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.video_tutorials
enum class FeatureType {
  kTest = -1,
  kInvalid = 0,
  kSummary = 1,
  kChromeIntro = 2,
  kDownload = 3,
  kSearch = 4,
  kVoiceSearch = 5,
  kMaxValue = kVoiceSearch,
};

// In memory struct of a video tutorial entry.
// Represents the metadata required to play a video tutorial.
struct Tutorial {
  Tutorial();
  Tutorial(FeatureType feature,
           const std::string& title,
           const std::string& video_url,
           const std::string& share_url,
           const std::string& poster_url,
           const std::string& animated_gif_url,
           const std::string& thumbnail_url,
           const std::string& caption_url,
           int video_length);
  ~Tutorial();

  bool operator==(const Tutorial& other) const;
  bool operator!=(const Tutorial& other) const;

  Tutorial(const Tutorial& other);
  Tutorial& operator=(const Tutorial& other);

  // Type of feature where this video tutorial targeted.
  FeatureType feature;

  // The title of the video.
  std::string title;

  // The URL of the video.
  GURL video_url;

  // The share URL for the video.
  GURL share_url;

  // The URL of the poster image. Shown while the video is loading in the
  // player.
  GURL poster_url;

  // The URL of the animated gif image.
  GURL animated_gif_url;

  // The URL of the video thumbnail.
  GURL thumbnail_url;

  // The URL of the subtitles.
  GURL caption_url;

  // The length of the video in seconds.
  int video_length;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_TUTORIAL_H_

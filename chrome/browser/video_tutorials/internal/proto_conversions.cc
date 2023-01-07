// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/proto_conversions.h"

#include "base/notreached.h"

namespace video_tutorials {

FeatureType ToFeatureType(proto::FeatureType type) {
  switch (type) {
    case proto::FeatureType::INVALID:
      return FeatureType::kInvalid;
    case proto::FeatureType::SUMMARY:
      return FeatureType::kSummary;
    case proto::FeatureType::CHROME_INTRO:
      return FeatureType::kChromeIntro;
    case proto::FeatureType::DOWNLOAD:
      return FeatureType::kDownload;
    case proto::FeatureType::SEARCH:
      return FeatureType::kSearch;
    case proto::FeatureType::VOICE_SEARCH:
      return FeatureType::kVoiceSearch;
    case proto::FeatureType::TEST:
      return FeatureType::kTest;
    default:
      return static_cast<FeatureType>(type);
  }
}

proto::FeatureType FromFeatureType(FeatureType type) {
  switch (type) {
    case FeatureType::kInvalid:
      return proto::FeatureType::INVALID;
    case FeatureType::kSummary:
      return proto::FeatureType::SUMMARY;
    case FeatureType::kChromeIntro:
      return proto::FeatureType::CHROME_INTRO;
    case FeatureType::kDownload:
      return proto::FeatureType::DOWNLOAD;
    case FeatureType::kSearch:
      return proto::FeatureType::SEARCH;
    case FeatureType::kVoiceSearch:
      return proto::FeatureType::VOICE_SEARCH;
    case FeatureType::kTest:
      return proto::FeatureType::TEST;
    default:
      return static_cast<proto::FeatureType>(type);
  }
}

void TutorialFromProto(const proto::VideoTutorial* proto, Tutorial* tutorial) {
  DCHECK(tutorial);
  DCHECK(proto);
  tutorial->feature = ToFeatureType(proto->feature());
  tutorial->title = proto->title();
  tutorial->video_url = GURL(proto->video_url());
  tutorial->share_url = GURL(proto->share_url());
  tutorial->poster_url = GURL(proto->poster_url());
  tutorial->animated_gif_url = GURL(proto->animated_gif_url());
  tutorial->thumbnail_url = GURL(proto->thumbnail_url());
  tutorial->caption_url = GURL(proto->caption_url());
  tutorial->video_length = proto->video_length();
}

std::vector<Tutorial> TutorialsFromProto(
    const proto::VideoTutorialGroup* proto) {
  std::vector<Tutorial> tutorials;
  DCHECK(proto);
  for (const auto& tutorial_proto : proto->tutorials()) {
    Tutorial tutorial;
    TutorialFromProto(&tutorial_proto, &tutorial);
    tutorials.emplace_back(std::move(tutorial));
  }
  return tutorials;
}

}  // namespace video_tutorials

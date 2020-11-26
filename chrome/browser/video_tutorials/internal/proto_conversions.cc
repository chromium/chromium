// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/proto_conversions.h"

#include "base/notreached.h"

namespace video_tutorials {
namespace {

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

}  // namespace

void TutorialToProto(Tutorial* tutorial, TutorialProto* proto) {
  DCHECK(tutorial);
  DCHECK(proto);
  proto->set_feature(FromFeatureType(tutorial->feature));
  proto->set_title(tutorial->title);
  proto->set_video_url(tutorial->video_url.spec());
  proto->set_share_url(tutorial->share_url.spec());
  proto->set_poster_url(tutorial->poster_url.spec());
  proto->set_animated_gif_url(tutorial->animated_gif_url.spec());
  proto->set_thumbnail_url(tutorial->thumbnail_url.spec());
  proto->set_caption_url(tutorial->caption_url.spec());
  proto->set_video_length(tutorial->video_length);
}

void TutorialFromProto(TutorialProto* proto, Tutorial* tutorial) {
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

void TutorialGroupToProto(TutorialGroup* group, TutorialGroupProto* proto) {
  DCHECK(group);
  DCHECK(proto);
  proto->set_language(group->language);
  proto->clear_tutorials();
  for (auto& tutorial : group->tutorials)
    TutorialToProto(&tutorial, proto->add_tutorials());
}

void TutorialGroupFromProto(TutorialGroupProto* proto, TutorialGroup* group) {
  DCHECK(group);
  DCHECK(proto);
  group->language = proto->language();
  group->tutorials.clear();
  for (auto tutorial_proto : proto->tutorials()) {
    Tutorial tutorial;
    TutorialFromProto(&tutorial_proto, &tutorial);
    group->tutorials.emplace_back(std::move(tutorial));
  }
}

void TutorialGroupsFromServerResponseProto(ServerResponseProto* proto,
                                           std::vector<TutorialGroup>* groups) {
  DCHECK(groups);
  DCHECK(proto);
  for (auto group_proto : proto->tutorial_groups()) {
    TutorialGroup group;
    TutorialGroupFromProto(&group_proto, &group);
    groups->emplace_back(group);
  }
}

}  // namespace video_tutorials

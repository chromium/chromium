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
    case proto::FeatureType::DEBUG:
      return FeatureType::kDebug;
    case proto::FeatureType::DOWNLOAD:
      return FeatureType::kDownload;
    case proto::FeatureType::SEARCH:
      return FeatureType::kSearch;
    case proto::FeatureType::TEST:
      return FeatureType::kTest;
    default:
      NOTREACHED();
  }
  return FeatureType::kInvalid;
}

proto::FeatureType FromFeatureType(FeatureType type) {
  switch (type) {
    case FeatureType::kInvalid:
      return proto::FeatureType::INVALID;
    case FeatureType::kDebug:
      return proto::FeatureType::DEBUG;
    case FeatureType::kDownload:
      return proto::FeatureType::DOWNLOAD;
    case FeatureType::kSearch:
      return proto::FeatureType::SEARCH;
    case FeatureType::kTest:
      return proto::FeatureType::TEST;
    default:
      NOTREACHED();
  }
  return proto::FeatureType::INVALID;
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
  tutorial->caption_url = GURL(proto->caption_url());
  tutorial->video_length = proto->video_length();
}

void TutorialGroupToProto(TutorialGroup* group, TutorialGroupProto* proto) {
  DCHECK(group);
  DCHECK(proto);
  proto->set_locale(group->locale);
  proto->clear_tutorials();
  for (auto& tutorial : group->tutorials)
    TutorialToProto(&tutorial, proto->add_tutorials());
}

void TutorialGroupFromProto(TutorialGroupProto* proto, TutorialGroup* group) {
  DCHECK(group);
  DCHECK(proto);
  group->locale = proto->locale();
  group->tutorials.clear();
  for (auto tutorial_proto : proto->tutorials()) {
    Tutorial tutorial;
    TutorialFromProto(&tutorial_proto, &tutorial);
    group->tutorials.emplace_back(std::move(tutorial));
  }
}

}  // namespace video_tutorials

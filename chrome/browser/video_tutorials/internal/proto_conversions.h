// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_PROTO_CONVERSIONS_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_PROTO_CONVERSIONS_H_

#include "chrome/browser/video_tutorials/internal/tutorial_group.h"
#include "chrome/browser/video_tutorials/proto/video_tutorials.pb.h"

namespace video_tutorials {

FeatureType ToFeatureType(proto::FeatureType type);
proto::FeatureType FromFeatureType(FeatureType type);

void TutorialFromProto(const proto::VideoTutorial* proto, Tutorial* tutorial);

// Convert proto::VideoTutorialGroup to in-memory std::vector<Tutorial>.
std::vector<Tutorial> TutorialsFromProto(
    const proto::VideoTutorialGroup* proto);

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_PROTO_CONVERSIONS_H_

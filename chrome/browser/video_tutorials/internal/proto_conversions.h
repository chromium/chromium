// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_PROTO_CONVERSIONS_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_PROTO_CONVERSIONS_H_

#include "chrome/browser/video_tutorials/internal/tutorial_group.h"
#include "chrome/browser/video_tutorials/proto/video_tutorials.pb.h"

namespace video_tutorials {

using TutorialProto = video_tutorials::proto::VideoTutorial;
using TutorialGroupProto = video_tutorials::proto::VideoTutorialGroup;

// Convert in-memory struct Tutorial to proto::VideoTutorial.
void TutorialToProto(Tutorial* tutorial, TutorialProto* proto);

// Convert proto::VideoTutorial to in-memory struct Tutorial.
void TutorialFromProto(TutorialProto* proto, Tutorial* tutorial);

// Convert in-memory struct TutorialGroup to proto::VideoTutorialGroup.
void TutorialGroupToProto(TutorialGroup* group, TutorialGroupProto* proto);

// Convert proto::VideoTutorialGroup to in-memory struct TutorialGroup.
void TutorialGroupFromProto(TutorialGroupProto* proto, TutorialGroup* group);

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_PROTO_CONVERSIONS_H_

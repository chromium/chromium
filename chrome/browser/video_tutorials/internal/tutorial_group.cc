// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_group.h"

namespace video_tutorials {

TutorialGroup::TutorialGroup() = default;

TutorialGroup::TutorialGroup(const std::string& locale) : locale(locale) {}

bool TutorialGroup::operator==(const TutorialGroup& other) const {
  return locale == other.locale && tutorials == other.tutorials;
}

bool TutorialGroup::operator!=(const TutorialGroup& other) const {
  return !(*this == other);
}

TutorialGroup::~TutorialGroup() = default;

TutorialGroup::TutorialGroup(const TutorialGroup& other) = default;

TutorialGroup& TutorialGroup::operator=(const TutorialGroup& other) = default;

}  // namespace video_tutorials

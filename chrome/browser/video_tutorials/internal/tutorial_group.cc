// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_group.h"

namespace video_tutorials {

TutorialGroup::TutorialGroup() = default;

TutorialGroup::TutorialGroup(const std::string& language)
    : language(language) {}

bool TutorialGroup::operator==(const TutorialGroup& other) const {
  return language == other.language && tutorials == other.tutorials;
}

bool TutorialGroup::operator!=(const TutorialGroup& other) const {
  return !(*this == other);
}

TutorialGroup::~TutorialGroup() = default;

TutorialGroup::TutorialGroup(const TutorialGroup& other) = default;

TutorialGroup& TutorialGroup::operator=(const TutorialGroup& other) = default;

}  // namespace video_tutorials

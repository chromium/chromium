// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_GROUP_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_GROUP_H_

#include <string>
#include <vector>

#include "chrome/browser/video_tutorials/tutorial.h"

namespace video_tutorials {

// In memory struct of a group of video tutorials with same language .
struct TutorialGroup {
  TutorialGroup();
  explicit TutorialGroup(const std::string& language);
  ~TutorialGroup();

  bool operator==(const TutorialGroup& other) const;
  bool operator!=(const TutorialGroup& other) const;

  TutorialGroup(const TutorialGroup& other);
  TutorialGroup& operator=(const TutorialGroup& other);

  // Language of this group.
  std::string language;

  // A list of tutorials.
  std::vector<Tutorial> tutorials;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_GROUP_H_

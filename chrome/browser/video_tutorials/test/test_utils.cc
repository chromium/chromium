// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/test/test_utils.h"

namespace video_tutorials {
namespace test {

const char kTestTitle[] = "Test Title";

void BuildTestEntry(Tutorial* entry) {
  *entry = Tutorial(
      FeatureType::kTest, kTestTitle, "https://www.example.com/video_url",
      "https://www.example.com/share_url", "https://www.example.com/poster_url",
      "https://www.example.com/animated_gif_url",
      "https://www.example.com/thumbnail_url",
      "https://www.example.com/caption_url", 60);
}

void BuildTestGroup(TutorialGroup* group) {
  *group = TutorialGroup("en");
  group->tutorials.clear();
  Tutorial entry1;
  BuildTestEntry(&entry1);
  group->tutorials.emplace_back(entry1);
  group->tutorials.emplace_back(entry1);
}

}  // namespace test
}  // namespace video_tutorials

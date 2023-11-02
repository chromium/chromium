// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_TEST_TEST_UTILS_H_

#include "chrome/browser/video_tutorials/internal/tutorial_group.h"

namespace video_tutorials {
namespace test {

// Build a TutorialGroup filled with fake data for test purpose.
void BuildTestGroup(TutorialGroup* group);

// Build a Tutorial entry filled with fake data for test purpose.
void BuildTestEntry(Tutorial* entry);

}  // namespace test
}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_TEST_TEST_UTILS_H_

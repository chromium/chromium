// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_SWITCHES_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_SWITCHES_H_

#include "base/feature_list.h"

namespace video_tutorials {
namespace features {

// Main feature flag for the video tutorials feature.
extern const base::Feature kVideoTutorials;

}  // namespace features

namespace switches {

// If set, the video tutorials will be fetched on startup.
extern const char kVideoTutorialsInstantFetch[];

}  // namespace switches

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_SWITCHES_H_

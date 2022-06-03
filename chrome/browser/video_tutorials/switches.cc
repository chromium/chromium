// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/switches.h"

namespace video_tutorials {
namespace features {

const base::Feature kVideoTutorials{"VideoTutorials",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace switches {

const char kVideoTutorialsInstantFetch[] = "video-tutorials-instant-fetch";

}  // namespace switches

}  // namespace video_tutorials

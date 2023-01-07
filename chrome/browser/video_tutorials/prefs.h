// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_PREFS_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_PREFS_H_


class PrefRegistrySimple;

namespace video_tutorials {

// Key for the locale that user picked in video tutorials service.
extern const char kPreferredLocaleKey[];

// Key to record a timestamp when the last update of video tutorials metadata
// happened.
extern const char kLastUpdatedTimeKey[];

// Register to prefs service.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_PREFS_H_

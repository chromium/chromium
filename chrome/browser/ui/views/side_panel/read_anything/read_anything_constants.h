// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_

// Various constants used throughout the Read Anything feature.
namespace {

// Visual constants for Read Anything feature.
const int kInternalInsets = 8;
const int kSeparatorTopBottomPadding = 4;

const int kButtonPadding = 8;
const int kSmallIconSize = 18;
const int kLargeIconSize = 20;

const char kReadAnythingDefaultFontName[] = "Standard font";

// Font size is stored in prefs as a (double) scaling factor, which is a number
// not presented to the user. The final font size sent to the UI is the default
// font size * font scale (in pixels).
const float kReadAnythingDefaultFontSize = 18.0f;
const double kReadAnythingDefaultFontScale = 1.0;
const double kReadAnythingMinimumFontScale = 0.2;
const double kReadAnythingMaximumFontScale = 5.0;

}  // namespace

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_

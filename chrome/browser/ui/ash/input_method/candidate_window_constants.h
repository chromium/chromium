// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_WINDOW_CONSTANTS_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_WINDOW_CONSTANTS_H_

namespace ui {
namespace ime {

// We'll use a bigger font size, so Chinese characters are more readable
// in the candidate window.
const int kFontSizeDelta = 2;

// Currently the infolist window only supports Japanese font.
const char kJapaneseFontName[] = "Noto Sans CJK JP";

// The minimum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too narrow when all candidates are short.
const int kMinCandidateLabelWidth = 100;
// The maximum width of candidate labels in the vertical candidate
// window. We use this value to prevent the candidate window from being
// too wide when one of candidates are long.
const int kMaxCandidateLabelWidth = 500;
// The minimum width of preedit area. We use this value to prevent the
// candidate window from being too narrow when candidate lists are not shown.
const int kMinPreeditAreaWidth = 134;

// The width of the infolist indicator icon in the candidate window.
const int kInfolistIndicatorIconWidth = 4;
// The padding size of the infolist indicator icon in the candidate window.
const int kInfolistIndicatorIconPadding = 2;

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_WINDOW_CONSTANTS_H_

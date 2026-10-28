// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TEXT_UTILS_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TEXT_UTILS_H_

namespace chrome_pdf {
struct AccessibilityCharInfo;
struct AccessibilityTextRunInfo;
}  // namespace chrome_pdf

namespace pdf {

// Returns whether `char_info` is a Unicode whitespace character.
bool IsWhitespace(const chrome_pdf::AccessibilityCharInfo& char_info);

// Returns whether `run` is horizontal left-to-right text.
bool IsLeftToRightRun(const chrome_pdf::AccessibilityTextRunInfo& run);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TEXT_UTILS_H_

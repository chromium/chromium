// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_text_utils.h"

#include "base/strings/string_util.h"
#include "pdf/accessibility_structs.h"

namespace pdf {

bool IsWhitespace(const chrome_pdf::AccessibilityCharInfo& char_info) {
  return base::IsUnicodeWhitespace(char_info.unicode_character);
}

bool IsLeftToRightRun(const chrome_pdf::AccessibilityTextRunInfo& run) {
  // `kNone` is assigned to single-character runs, whose direction cannot be
  // inferred. Treat them as LTR, the default, like `PDFiumPage` does.
  return run.direction ==
             chrome_pdf::AccessibilityTextDirection::kLeftToRight ||
         run.direction == chrome_pdf::AccessibilityTextDirection::kNone;
}

}  // namespace pdf

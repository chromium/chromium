// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/accessibility/read_anything_constants.h"

namespace string_constants {

// Used as an initial value in the model. This is not shown to the user.
// The font option shown to the user before a selection occurs is either from
// their saved preference or from the default selected_index_ in the font model.
const char kReadAnythingPlaceholderFontName[] = "Poppins";

// Used as an initial value in prefs. This is not shown to the user.
const char kReadAnythingPlaceholderVoiceName[] = "";

const char kLetterSpacingHistogramName[] =
    "Accessibility.ReadAnything.LetterSpacing";
const char kLineSpacingHistogramName[] =
    "Accessibility.ReadAnything.LineSpacing";
const char kColorHistogramName[] = "Accessibility.ReadAnything.Color";
const char kFontNameHistogramName[] = "Accessibility.ReadAnything.FontName";
const char kFontScaleHistogramName[] = "Accessibility.ReadAnything.FontScale";
const char kSettingsChangeHistogramName[] =
    "Accessibility.ReadAnything.SettingsChange";
const char kScrollEventHistogramName[] =
    "Accessibility.ReadAnything.ScrollEvent";
const char kEmptyStateHistogramName[] = "Accessibility.ReadAnything.EmptyState";
const char kLanguageHistogramName[] = "Accessibility.ReadAnything.Language";
const char kPDFPageEnd[] = "End of extracted text";
const char kPDFPageStart[] = "Start of extracted text";

const std::set<std::string> GetNonSelectableUrls() {
  return {
      "https://docs.google.com/document*",
      "https://docs.sandbox.google.com/*",
  };
}

}  // namespace string_constants

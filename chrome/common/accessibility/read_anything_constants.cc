// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/accessibility/read_anything_constants.h"

namespace string_constants {

// Used as an initial value in the model. This is not shown to the user.
// The font option shown to the user before a selection occurs is either from
// their saved preference or from the default selected_index_ in the font model.
const char kReadAnythingPlaceholderFontName[] = "Poppins";

// Used as a fallback font for css if the current font is unavailable or
// invalid.
const char kReadAnythingDefaultFont[] = "sans-serif";

// Used as an initial value in prefs. This is not shown to the user.
const char kReadAnythingPlaceholderVoiceName[] = "";

const char kLetterSpacingHistogramName[] =
    "Accessibility.ReadAnything.LetterSpacing";
const char kLineSpacingHistogramName[] =
    "Accessibility.ReadAnything.LineSpacing";
const char kColorHistogramName[] = "Accessibility.ReadAnything.Color";
const char kFontNameHistogramName[] = "Accessibility.ReadAnything.FontName";
const char kFontScaleHistogramName[] = "Accessibility.ReadAnything.FontScale";
const char kScrollEventHistogramName[] =
    "Accessibility.ReadAnything.ScrollEvent";
const char kEmptyStateHistogramName[] = "Accessibility.ReadAnything.EmptyState";
const char kLanguageHistogramName[] = "Accessibility.ReadAnything.Language";

}  // namespace string_constants

namespace fonts {

const base::fixed_flat_map<std::string_view, FontInfo, 9> kFontInfos =
    base::MakeFixedFlatMap<std::string_view, FontInfo>({
        {"Poppins", kPoppinsFontInfo},
        {"Sans-serif", kSansSerifFontInfo},
        {"Serif", kSerifFontInfo},
        {"Comic Neue", kComicNeueFontInfo},
        {"Lexend Deca", kLexendDecaFontInfo},
        {"EB Garamond", kEbGaramondFontInfo},
        {"STIX Two Text", kStixTwoTextFontInfo},
        {"Andika", kAndikaFontInfo},
        {"Atkinson Hyperlegible", kAtkinsonHyperlegibleFontInfo},
    });

}  // namespace fonts

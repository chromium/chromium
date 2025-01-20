// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_CONSTANTS_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_CONSTANTS_H_

namespace translate {

// The maximum number of characters allowed for a text selection in Partial
// Translate. Longer selections will be truncated down to the first valid word
// break respecting the threshold.
extern const int kDesktopPartialTranslateTextSelectionMaxCharacters;
// The number of milliseconds to wait before showing the Partial Translate
// bubble, even if no response has been received. In this case, a waiting view
// is shown.
extern const int kDesktopPartialTranslateBubbleShowDelayMs;

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_CONSTANTS_H_

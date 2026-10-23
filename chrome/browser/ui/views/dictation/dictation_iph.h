// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_H_

#include "ui/base/interaction/element_identifier.h"

namespace dictation {

// Identifies the anchor that tracks the focused text field. The dictation IPH
// is registered against this identifier.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDictationIphAnchorElementId);

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TYPE_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TYPE_H_

#include <string>

#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// These identifiers serve as stable, semantic identifiers for UI elements.
// These are not unique, analogous to CSS classes.
enum UiElementType {
  kTypeNone = 0,
  kTypeScaledDepthAdjuster,
  kTypePromptBackground,
  kTypePromptIcon,
  kTypePromptText,
  kTypeSpacer,

  // This must be last.
  kNumUiElementTypes,
};

VR_UI_EXPORT std::string UiElementTypeToString(UiElementType type);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TYPE_H_

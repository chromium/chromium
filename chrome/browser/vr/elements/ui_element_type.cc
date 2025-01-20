// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element_type.h"

#include <array>

#include "base/check_op.h"

namespace vr {

namespace {

// LINT.IfChange(UiElementType)
static std::array<const char*, kNumUiElementTypes> g_ui_element_type_strings = {
    "kTypeNone",       "kTypeScaledDepthAdjuster", "kTypePromptBackground",
    "kTypePromptIcon", "kTypePromptText",          "kTypeSpacer",
};
// LINT.ThenChange(//chrome/browser/vr/elements/ui_element_type.h:UiElementType)

}  // namespace

std::string UiElementTypeToString(UiElementType type) {
  CHECK_GT(kNumUiElementTypes, type);
  return g_ui_element_type_strings[type];
}

}  // namespace vr

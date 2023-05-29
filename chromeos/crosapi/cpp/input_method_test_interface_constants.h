// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_INPUT_METHOD_TEST_INTERFACE_CONSTANTS_H_
#define CHROMEOS_CROSAPI_CPP_INPUT_METHOD_TEST_INTERFACE_CONSTANTS_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"

namespace crosapi {

COMPONENT_EXPORT(CROSAPI)
inline constexpr base::StringPiece kInputMethodTestCapabilitySendKeyModifiers =
    "SendKeyEventModifiers";

COMPONENT_EXPORT(CROSAPI)
inline constexpr base::StringPiece
    kInputMethodTestCapabilityConfirmComposition = "ConfirmComposition";

// When the input method wants to commit the composition, always call
// ConfirmCompositionText even if Ash thinks there's no composition.
// Fixes b/265853952.
COMPONENT_EXPORT(CROSAPI)
inline constexpr base::StringPiece
    kInputMethodTestCapabilityAlwaysConfirmComposition =
        "AlwaysConfirmComposition";

COMPONENT_EXPORT(CROSAPI)
inline constexpr base::StringPiece
    kInputMethodTestCapabilityDeleteSurroundingText = "DeleteSurroundingText";

COMPONENT_EXPORT(CROSAPI)
inline constexpr base::StringPiece
    kInputMethodTestCapabilityExtendedConfirmComposition =
        "ExtendedConfirmComposition";

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_INPUT_METHOD_TEST_INTERFACE_CONSTANTS_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_OVERLAY_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_OVERLAY_STATE_H_

#include "base/callback.h"

namespace autofill_assistant {

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.overlay)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantOverlayState
enum OverlayState {
  // The overlay is completely hidden.
  HIDDEN = 0,

  // The overlay is enabled and covers the whole web page.
  FULL = 1,

  // The overlay is enabled but some portions of the web page might still be
  // accessible.
  PARTIAL = 2,
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_OVERLAY_STATE_H_

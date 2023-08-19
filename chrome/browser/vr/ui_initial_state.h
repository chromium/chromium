// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INITIAL_STATE_H_
#define CHROME_BROWSER_VR_UI_INITIAL_STATE_H_

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

// This class describes the initial state of a UI, and may be used by a UI
// instances owner to specify a custom state on startup.
struct VR_BASE_EXPORT UiInitialState {
  UiInitialState();
  UiInitialState(const UiInitialState& other);
  bool gvr_input_support = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INITIAL_STATE_H_

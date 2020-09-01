// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BOTTOM_SHEET_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BOTTOM_SHEET_STATE_H_

#include <ostream>

namespace autofill_assistant {

// See definition in
// components/browser_ui/bottomsheet/BottomSheetController.java
enum class BottomSheetState {
  UNDEFINED = 0,
  COLLAPSED = 1,
  EXPANDED = 2,
};

std::ostream& operator<<(std::ostream& out, const BottomSheetState& state);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BOTTOM_SHEET_STATE_H_
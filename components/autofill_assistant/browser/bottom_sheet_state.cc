// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/bottom_sheet_state.h"

namespace autofill_assistant {

std::ostream& operator<<(std::ostream& out, const BottomSheetState& state) {
#ifdef NDEBUG
  out << static_cast<int>(state);
  return out;
#else
  switch (state) {
    case BottomSheetState::UNDEFINED:
      out << "UNDEFINED";
      break;
    case BottomSheetState::COLLAPSED:
      out << "COLLAPSED";
      break;
    case BottomSheetState::EXPANDED:
      out << "EXPANDED";
      break;
  }
  return out;
#endif
}

}  // namespace autofill_assistant

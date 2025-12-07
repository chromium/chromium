// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/input_method/mock_assistive_delegate.h"

#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"

namespace ui::ime {
void MockAssistiveDelegate::AssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) const {
  last_window_type_ = button.window_type;
}
ash::ime::AssistiveWindowType MockAssistiveDelegate::last_window_type_ =
    ash::ime::AssistiveWindowType::kNone;
}  // namespace ui::ime

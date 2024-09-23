// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_MOCK_ASSISTIVE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_MOCK_ASSISTIVE_DELEGATE_H_

namespace ui::ime {

class MockAssistiveDelegate : public AssistiveDelegate {
 public:
  ~MockAssistiveDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override;
  void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) const override {}
  static ash::ime::AssistiveWindowType last_window_type_;
};
}  // namespace ui::ime

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_MOCK_ASSISTIVE_DELEGATE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_MOCK_ASSISTIVE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_MOCK_ASSISTIVE_DELEGATE_H_

namespace ui::ime {

class MockAssistiveDelegate : public AssistiveDelegate {
 public:
  MockAssistiveDelegate();
  ~MockAssistiveDelegate() override;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override;
  void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) const override {}
  base::WeakPtr<AssistiveDelegate> GetWeakPtr() override;
  static ash::ime::AssistiveWindowType last_window_type_;

 private:
  base::WeakPtrFactory<MockAssistiveDelegate> weak_ptr_factory_{this};
};
}  // namespace ui::ime

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_MOCK_ASSISTIVE_DELEGATE_H_

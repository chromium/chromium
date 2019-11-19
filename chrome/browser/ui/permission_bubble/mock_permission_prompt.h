// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERMISSION_BUBBLE_MOCK_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_PERMISSION_BUBBLE_MOCK_PERMISSION_PROMPT_H_

#include "chrome/browser/ui/permission_bubble/permission_prompt.h"

class MockPermissionPromptFactory;

// Provides a skeleton class for unit and browser testing when trying to test
// the request manager logic. Should not be used for anything that requires
// actual UI.
// Use the MockPermissionPromptFactory to create this.
class MockPermissionPrompt : public PermissionPrompt {
 public:
  ~MockPermissionPrompt() override;

  // PermissionPrompt:
  void UpdateAnchorPosition() override;
  gfx::NativeWindow GetNativeWindow() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;

  bool IsVisible();

 private:
  friend class MockPermissionPromptFactory;

  MockPermissionPrompt(MockPermissionPromptFactory* factory,
                       Delegate* delegate);

  MockPermissionPromptFactory* factory_;
  Delegate* delegate_;
};

#endif  // CHROME_BROWSER_UI_PERMISSION_BUBBLE_MOCK_PERMISSION_PROMPT_H_

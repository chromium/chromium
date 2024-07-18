// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_H_

#include "base/memory/raw_ptr.h"
#include "components/permissions/permission_prompt.h"

namespace permissions {
class MockPermissionPromptFactory;

// Provides a skeleton class for unit and browser testing when trying to test
// the request manager logic. Should not be used for anything that requires
// actual UI.
// Use the MockPermissionPromptFactory to create this.
class MockPermissionPrompt : public PermissionPrompt {
 public:
  ~MockPermissionPrompt() override;

  // PermissionPrompt:
  bool UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  PermissionPromptDisposition GetPromptDisposition() const override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  std::optional<feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const override;
  bool IsAskPrompt() const override;

  bool IsVisible();

 private:
  friend class MockPermissionPromptFactory;

  MockPermissionPrompt(MockPermissionPromptFactory* factory,
                       Delegate* delegate);

  raw_ptr<MockPermissionPromptFactory> factory_;
  raw_ptr<Delegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_PROMPT_H_

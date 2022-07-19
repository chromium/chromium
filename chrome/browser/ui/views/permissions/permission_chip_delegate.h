// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_CHIP_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_CHIP_DELEGATE_H_

#include <string>
#include "components/permissions/permission_prompt.h"

namespace views {
class View;
}

namespace gfx {
struct VectorIcon;
}

enum class OmniboxChipTheme;

// The delegate interface provides data for a visual representation of the
// permission prompt in the form of a quiet or normal permission chip.
class PermissionChipDelegate {
 public:
  virtual ~PermissionChipDelegate() = default;
  // Returns a newly-created permission prompt bubble.
  [[nodiscard]] virtual views::View* CreateBubble() = 0;

  // Show previously created prompt bubble.
  virtual void ShowBubble() = 0;

  virtual const gfx::VectorIcon& GetIconOn() = 0;
  virtual const gfx::VectorIcon& GetIconOff() = 0;
  virtual std::u16string GetMessage() = 0;
  virtual bool ShouldStartOpen() = 0;
  virtual bool ShouldExpand() = 0;
  virtual OmniboxChipTheme GetTheme() = 0;
  virtual permissions::PermissionPrompt::Delegate*
  GetPermissionPromptDelegate() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_CHIP_DELEGATE_H_

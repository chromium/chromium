// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_SHOW_SYSTEM_PROMPT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_SHOW_SYSTEM_PROMPT_VIEW_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"

class Browser;

// A view used to display information to the user that they need to grant
// permission to Chrome on a system level.
class EmbeddedPermissionPromptShowSystemPromptView
    : public EmbeddedPermissionPromptBaseView {
 public:
  EmbeddedPermissionPromptShowSystemPromptView(
      Browser* browser,
      base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate);
  EmbeddedPermissionPromptShowSystemPromptView(
      const EmbeddedPermissionPromptShowSystemPromptView&) = delete;
  EmbeddedPermissionPromptShowSystemPromptView& operator=(
      const EmbeddedPermissionPromptShowSystemPromptView&) = delete;
  ~EmbeddedPermissionPromptShowSystemPromptView() override;

  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  bool ShowLoadingIcon() const override;
  void RunButtonCallback(int type) override;

 protected:
  std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const override;
  std::vector<ButtonConfiguration> GetButtonsConfiguration() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_SHOW_SYSTEM_PROMPT_VIEW_H_

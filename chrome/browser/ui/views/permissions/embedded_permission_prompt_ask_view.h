// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_ASK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_ASK_VIEW_H_
#include <string>

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

// View that prompts the user to grant or deny a permission request from one
// origin when the user has not previously made a decision.
class EmbeddedPermissionPromptAskView
    : public EmbeddedPermissionPromptBaseView {
  METADATA_HEADER(EmbeddedPermissionPromptAskView,
                  EmbeddedPermissionPromptBaseView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowThisTimeId);

  EmbeddedPermissionPromptAskView(
      Browser* browser,
      base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate);
  EmbeddedPermissionPromptAskView(const EmbeddedPermissionPromptAskView&) =
      delete;
  EmbeddedPermissionPromptAskView& operator=(
      const EmbeddedPermissionPromptAskView&) = delete;
  ~EmbeddedPermissionPromptAskView() override;

  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  void RunButtonCallback(int type) override;

 protected:
  std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const override;
  std::vector<ButtonConfiguration> GetButtonsConfiguration() const override;

 private:
  std::u16string GetMessageText() const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_ASK_VIEW_H_

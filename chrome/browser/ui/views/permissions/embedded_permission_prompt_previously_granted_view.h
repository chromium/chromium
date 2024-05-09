// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_PREVIOUSLY_GRANTED_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_PREVIOUSLY_GRANTED_VIEW_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"

class Browser;

// A view used to display information to the user that they previously granted
// permission to a site.
class EmbeddedPermissionPromptPreviouslyGrantedView
    : public EmbeddedPermissionPromptBaseView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContinueAllowingId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kStopAllowingId);
  EmbeddedPermissionPromptPreviouslyGrantedView(
      Browser* browser,
      base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate);
  EmbeddedPermissionPromptPreviouslyGrantedView(
      const EmbeddedPermissionPromptPreviouslyGrantedView&) = delete;
  EmbeddedPermissionPromptPreviouslyGrantedView& operator=(
      const EmbeddedPermissionPromptPreviouslyGrantedView&) = delete;
  ~EmbeddedPermissionPromptPreviouslyGrantedView() override;

  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  void RunButtonCallback(int type) override;

 protected:
  std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const override;
  std::vector<ButtonConfiguration> GetButtonsConfiguration() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_PREVIOUSLY_GRANTED_VIEW_H_

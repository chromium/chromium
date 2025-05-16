// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace ash {
// This dialog is shown at startup during the delay in which floating
// workspace fetches the state.
class FloatingWorkspaceDialog : public SystemWebDialogDelegate {
 public:
  FloatingWorkspaceDialog(const FloatingWorkspaceDialog&) = delete;
  FloatingWorkspaceDialog& operator=(const FloatingWorkspaceDialog&) = delete;
  ~FloatingWorkspaceDialog() override;

  // Displays the dialog.
  static void Show();

  // Closes the dialog if it's currently opened.
  static void Close();

 protected:
  FloatingWorkspaceDialog();

  // ui::WebDialogDelegate overrides
  void GetDialogSize(gfx::Size* size) const override;
  void OnDialogClosed(const std::string& json_retval) override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCloseDialogOnEscape() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_DIALOG_H_

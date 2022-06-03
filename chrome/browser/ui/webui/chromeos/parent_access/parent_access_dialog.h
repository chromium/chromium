// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos {

// Dialog which embeds the Parent Access UI, which verifies a parent during
// a child session.
class ParentAccessDialog : public SystemWebDialogDelegate {
 public:
  // Error state returned by the Show() function.
  enum ShowError { kNone, kDialogAlreadyVisible, kNotAChildUser };

  // Shows the dialog; if the dialog is already displayed, this returns an
  // error.
  // TODO(b/200853161): Add parameter which is passed over the the Mojo bridge.
  static ShowError Show();

  static ParentAccessDialog* GetInstance();

  explicit ParentAccessDialog(const ParentAccessDialog&) = delete;
  ParentAccessDialog& operator=(const ParentAccessDialog&) = delete;

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldCloseDialogOnEscape() const override;

 protected:
  // TODO(b/200853161): Add parameter which is passed over the the Mojo bridge.
  ParentAccessDialog();
  ~ParentAccessDialog() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
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
  static ShowError Show(parent_access_ui::mojom::ParentAccessParamsPtr params);

  static ParentAccessDialog* GetInstance();

  explicit ParentAccessDialog(const ParentAccessDialog&) = delete;
  ParentAccessDialog& operator=(const ParentAccessDialog&) = delete;

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldCloseDialogOnEscape() const override;

  // Makes a copy of the ParentAccessParams. The ParentAccessDialog should
  // maintain one copy of the parent_access_params_ object, which is why a clone
  // is made, instead of transferring ownership to the caller.
  parent_access_ui::mojom::ParentAccessParamsPtr CloneParentAccessParams();

  parent_access_ui::mojom::ParentAccessParams* GetParentAccessParamsForTest();

 protected:
  explicit ParentAccessDialog(
      parent_access_ui::mojom::ParentAccessParamsPtr params);
  ~ParentAccessDialog() override;

 private:
  parent_access_ui::mojom::ParentAccessParamsPtr parent_access_params_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

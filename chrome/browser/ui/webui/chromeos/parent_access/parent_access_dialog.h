// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos {

// Dialog which embeds the Parent Access UI, which verifies a parent during
// a child session.
class ParentAccessDialog : public SystemWebDialogDelegate {
 public:
  // Error state returned by the Show() function.
  enum ShowError { kNone, kDialogAlreadyVisible, kNotAChildUser };

  // The result of the parent access request, passed back to the caller.
  struct Result {
    // The status of the result.
    enum Status {
      kApproved,   // The parent was verified and they approved.
      kDeclined,   // The request was explicitly declined by the parent.
      kCancelled,  // The request was cancelled/dismissed by the parent.
      kError,      // An error occurred while handling the request.
    };
    Status status = kCancelled;

    // The Parent Access Token.  Only set if status is kVerified.
    std::string parent_access_token = "";
  };

  // Callback for the result of the dialog.
  using ParentAccessDialogCallback =
      base::OnceCallback<void(std::unique_ptr<Result>)>;

  // Shows the dialog; if the dialog is already displayed, this returns an
  // error.
  static ShowError Show(parent_access_ui::mojom::ParentAccessParamsPtr params,
                        ParentAccessDialogCallback callback);

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

  // Used by the ParentAccessUI to set the result of the Parent Access
  // request and close the dialog.
  void SetResultAndClose(std::unique_ptr<ParentAccessDialog::Result> result);

  parent_access_ui::mojom::ParentAccessParams* GetParentAccessParamsForTest();

 protected:
  explicit ParentAccessDialog(
      parent_access_ui::mojom::ParentAccessParamsPtr params,
      ParentAccessDialogCallback callback);
  ~ParentAccessDialog() override;

 private:
  parent_access_ui::mojom::ParentAccessParamsPtr parent_access_params_;
  ParentAccessDialogCallback callback_;

  // The Parent Access result.  Set by the ParentAccessUI
  std::unique_ptr<Result> result_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

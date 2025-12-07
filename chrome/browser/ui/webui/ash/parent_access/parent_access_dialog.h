// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui_handler_delegate.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace ash {

class WindowDimmer;

// Dialog which embeds the Parent Access UI, which verifies a
// parent during a child session.
class ParentAccessDialog : public ParentAccessUiHandlerDelegate,
                           public SystemWebDialogDelegate {
 public:
  struct Result {
    // The status of the result.
    enum class Status {
      kApproved,  // The parent was verified and they approved.
      kDeclined,  // The request was explicitly declined by the parent.
      kCanceled,  // The request was canceled/dismissed by the parent.
      kDisabled,  // Making a request has been disabled by the parent.
      kError,     // An error occurred while handling the request.
    };
    Status status = Status::kCanceled;

    // The Parent Access Token.  Only set if status is kVerified.
    std::string parent_access_token = "";

    // The UTC timestamp at which the token expires.
    base::Time parent_access_token_expire_timestamp;
  };

  // Callback for the result of the dialog.
  using Callback = base::OnceCallback<void(std::unique_ptr<Result>)>;

  static ParentAccessDialog* GetInstance();

  explicit ParentAccessDialog(const ParentAccessDialog&) = delete;
  ParentAccessDialog& operator=(const ParentAccessDialog&) = delete;

  // ui::WebDialogDelegate:
  ui::mojom::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldShowCloseButton() const override;

  // ParentAccessUiHandlerDelegate:
  parent_access_ui::mojom::ParentAccessParamsPtr CloneParentAccessParams()
      override;
  void SetApproved(const std::string& parent_access_token,
                   const base::Time& expire_timestamp) override;
  void SetDeclined() override;
  void SetCanceled() override;
  void SetDisabled() override;
  void SetError() override;

  parent_access_ui::mojom::ParentAccessParams* GetParentAccessParamsForTest()
      const;

  explicit ParentAccessDialog(
      parent_access_ui::mojom::ParentAccessParamsPtr params,
      Callback callback);

  // Creates and shows additional dimmer underneath the dialog.
  void ShowDimmer();

 protected:
  ~ParentAccessDialog() override;

 private:
  void CloseWithResult(std::unique_ptr<Result> result);

  parent_access_ui::mojom::ParentAccessParamsPtr parent_access_params_;
  Callback callback_;

  // The dimmer shown underneath the dialog in order to mitigate spoofing by the
  // malicious website. The dimmer clearly renders over the browser UI.
  std::unique_ptr<WindowDimmer> dimmer_;

  // The Parent Access Dialog result passed back to the caller when the dialog
  // completes.
  std::unique_ptr<Result> result_;
};

// Interface that provides the ParentAccessDialog to external clients.
// The provider should be used to show the dialog.  The default implementation
// is can be overridden by tests to provide a fake implementation like this:
//
// class FakeParentAccessDialogProvider
//    : public ash::ParentAccessDialogProvider {
// public:
//  ParentAccessDialogProvider::ShowError Show(
//      parent_access_ui::mojom::ParentAccessParamsPtr params,
//      ash::ParentAccessDialog::Callback callback) override {}
// }
class ParentAccessDialogProvider {
 public:
  // Error state returned by the Show() function.
  enum class ShowError { kNone, kDialogAlreadyVisible, kNotAChildUser };

  ParentAccessDialogProvider() = default;
  ParentAccessDialogProvider(const ParentAccessDialogProvider& other) = delete;
  ParentAccessDialogProvider& operator=(
      const ParentAccessDialogProvider& other) = delete;
  virtual ~ParentAccessDialogProvider() = default;

  // Shows the dialog. If the dialog is already displayed, this returns an
  // error.  virtual so it can be overridden for tests to fake dialog behavior.
  virtual ShowError Show(parent_access_ui::mojom::ParentAccessParamsPtr params,
                         ParentAccessDialog::Callback callback);

  // Used for metrics. Those values are logged to UMA. Entries should not be
  // renumbered and numeric values should never be reused. Please keep in sync
  // with "FamilyLinkUserParentAccessWidgetShowDialogError" in
  // src/tools/metrics/histograms/enums.xml.
  enum class ShowErrorType {
    kUnknown = 0,
    kAlreadyVisible = 1,
    kNotAChildUser = 2,
    kMaxValue = kNotAChildUser
  };
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_DIALOG_H_

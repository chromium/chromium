// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_H_
#define CHROME_BROWSER_UI_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_H_

#include <stddef.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions {
class Extension;
}

// This file provides an API that will prompt a child user's parent(s) for their
// permission for action.  The parent(s) can approve the action by entering
// their Google password, which is then verified using the Google
// Reauthentication API's child to parent delegation mode.  The prompt can only
// be shown if the user is a child.  Otherwise, the prompt will fail.

// Clients should provide a ParentPermissionDialog::DoneCallback to
// receive the results of the dialog.
// Example Usage:
// ParentPermissionDialog::DoneCallback callback = base::BindOnce(
//            &MyClass::OnParentPermissionDialogDone,
//            weak_ptr_factory_.GetWeakPtr()))
// gfx::ImageSkia icon = LoadMyIcon();
//
// std::unique_ptr<ParentPermissionDialog> dialog =
// CreateParentPermissionDialog(profile, window, icon, message, done_callback);
//
// dialog->ShowDialog();
//
// MyClass::ParentPermissionDialogDone(ParentPermissionDialog::Result result) {
//   switch (result) {
//     ...
//   }
// }

// API for the Dialog.
class ParentPermissionDialog {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDialogViewIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kExtensionsParentApprovalVerificationTextIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kParentAccountTextIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIncorrectParentPasswordIdForTesting);

  enum class Result {
    // The parent has given their permission for the action.
    kParentPermissionReceived,
    // The dialog was canceled.
    kParentPermissionCanceled,
    // Parent Permission was attempted, but failed due to an unrecoverable
    // error,  i.e. a network error.
    // NOTE: This does not indicate that the password entered was incorrect.
    kParentPermissionFailed,
  };

  virtual ~ParentPermissionDialog() = default;

  // Shows the Dialog. The process to show it can be asynchronous, so the dialog
  // may not appear immediately.
  virtual void ShowDialog() = 0;

  // Type of the callback invoked with the dialog completes.
  using DoneCallback = base::OnceCallback<void(Result result)>;

  // Creates a ParentPermissionDialog.
  // |profile| is the child user's profile.
  // |window| is the window to which the dialog will be modal. Can be nullptr.
  // |icon| will be displayed to the side of |message|.
  // |message| will be displayed in the body of the dialog.
  // |done_callback| will be called  on dialog completion.
  static std::unique_ptr<ParentPermissionDialog> CreateParentPermissionDialog(
      Profile* profile,
      gfx::NativeWindow window,
      const gfx::ImageSkia& icon,
      const std::u16string& message,
      ParentPermissionDialog::DoneCallback done_callback);

  // Creates a ParentPermissionDialog customized for the installation of the
  // specified |extension|.
  // |profile| is the child user's profile.
  // |window| is the window to which the dialog will be modal. Can be nullptr.
  // |icon| will be used as a backup in case |extension| has not loaded.
  // |extension_approval_entry_point| indicates which flow invoked the dialog.
  // |done_callback| will be called  on dialog completion.
  static std::unique_ptr<ParentPermissionDialog>
  CreateParentPermissionDialogForExtension(
      Profile* profile,
      gfx::NativeWindow window,
      const gfx::ImageSkia& icon,
      const extensions::Extension* extension,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ParentPermissionDialog::DoneCallback done_callback);
};

#endif  // CHROME_BROWSER_UI_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_H_

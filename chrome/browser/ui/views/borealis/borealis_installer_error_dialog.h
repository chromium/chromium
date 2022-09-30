// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_ERROR_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "ui/gfx/native_widget_types.h"

namespace views::borealis {

// When the user dismisses the error dialog, they will do so with one of these
// choices.
enum class ErrorDialogChoice {
  kRetry,  // User wants to re-try the installation
  kExit,   // User wants to exit the installation
};

// This callback is used to communicate the user's choice in the error dialog to
// the code that created the dialog.
using DialogCallback = base::OnceCallback<void(ErrorDialogChoice)>;

// Shows a dialog that indicates a borealis installation error |result| has
// occurred. This dialog will be modal to the given |parent|. When the user
// closes the dialog, |callback| will be invoked with their choice of how to
// proceed.
void ShowInstallerErrorDialog(gfx::NativeView parent,
                              ::borealis::BorealisInstallResult result,
                              DialogCallback callback);

}  // namespace views::borealis

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_INSTALLER_ERROR_DIALOG_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_FACTORY_H_

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

class PasswordsLeakDialogDelegate;
namespace credential_leak_dialog_factory {

// Factory function for creating CredentialLeakDialogController objects.
std::unique_ptr<CredentialLeakDialogController> CreateController(
    PasswordsLeakDialogDelegate* delegate,
    password_manager::metrics_util::LeakDialogType dialog_type);

}  // namespace credential_leak_dialog_factory

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_LEAK_DIALOG_CONTROLLER_FACTORY_H_

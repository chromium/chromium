// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_factory.h"

#include "chrome/browser/ui/passwords/credential_leak_dialog_change_controller.h"
namespace {
using password_manager::metrics_util::LeakDialogType;
}
namespace credential_leak_dialog_factory {
std::unique_ptr<CredentialLeakDialogController> CreateController(
    PasswordsLeakDialogDelegate* delegate,
    LeakDialogType dialog_type) {
  // TODO(crbug/1317972): implement factory logic to instantiate the different
  // dialog types.
  switch (dialog_type) {
    case LeakDialogType::kCheckup:
      return nullptr;
    case LeakDialogType::kChange:
      return std::make_unique<CredentialLeakDialogChangeController>(
          delegate, dialog_type);
    case LeakDialogType::kChangeAutomatically:
      return nullptr;
    case LeakDialogType::kCheckupAndChange:
      return nullptr;
  }
}

}  // namespace credential_leak_dialog_factory

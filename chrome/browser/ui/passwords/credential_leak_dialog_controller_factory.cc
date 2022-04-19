// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_factory.h"

namespace credential_leak_dialog_factory {

std::unique_ptr<CredentialLeakDialogController> CreateController(
    password_manager::metrics_util::LeakDialogType dialog_type) {
  // TODO(crbug/1313893): implement factory logic to instantiate the different
  // dialog types.
  return nullptr;
}

}  // namespace credential_leak_dialog_factory

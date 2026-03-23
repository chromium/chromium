// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/remove_local_auth_factors_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

RemoveLocalAuthFactorsScreenHandler::RemoveLocalAuthFactorsScreenHandler()
    : BaseScreenHandler(kScreenId) {}

RemoveLocalAuthFactorsScreenHandler::~RemoveLocalAuthFactorsScreenHandler() =
    default;

// TODO: b/445628245 - add localized values.
void RemoveLocalAuthFactorsScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void RemoveLocalAuthFactorsScreenHandler::Show() {
  ShowInWebUI();
}

void RemoveLocalAuthFactorsScreenHandler::
    ShowRemoveLocalAuthFactorsSuccessStep() {
  CallExternalAPI("showRemoveLocalAuthFactorsSuccessStep");
}

base::WeakPtr<RemoveLocalAuthFactorsScreenView>
RemoveLocalAuthFactorsScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/tuna_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

TunaScreenHandler::TunaScreenHandler() : BaseScreenHandler(kScreenId) {}

TunaScreenHandler::~TunaScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void TunaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
}

void TunaScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<TunaScreenView> TunaScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

PerksDiscoveryScreenHandler::PerksDiscoveryScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PerksDiscoveryScreenHandler::~PerksDiscoveryScreenHandler() = default;

void PerksDiscoveryScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void PerksDiscoveryScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<PerksDiscoveryScreenView>
PerksDiscoveryScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash

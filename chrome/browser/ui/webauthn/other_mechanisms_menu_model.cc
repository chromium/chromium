// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/other_mechanisms_menu_model.h"

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

OtherMechanismsMenuModel::OtherMechanismsMenuModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : ui::SimpleMenuModel(this), dialog_model_(dialog_model) {
  base::span<const AuthenticatorRequestDialogModel::Mechanism> mechanisms =
      dialog_model->mechanisms();
  const absl::optional<size_t> current_mechanism =
      dialog_model->current_mechanism();

  constexpr int kTransportIconSize = 16;
  for (size_t i = 0; i < mechanisms.size(); i++) {
    if (current_mechanism && i == *current_mechanism) {
      continue;
    }

    const auto& m = mechanisms[i];
    AddItemWithIcon(static_cast<int>(i), m.short_name,
                    ui::ImageModel::FromVectorIcon(*m.icon, ui::kColorMenuIcon,
                                                   kTransportIconSize));
  }
}

OtherMechanismsMenuModel::~OtherMechanismsMenuModel() = default;

bool OtherMechanismsMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool OtherMechanismsMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void OtherMechanismsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  dialog_model_->mechanisms()[command_id].callback.Run();
}

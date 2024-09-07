// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

namespace {

std::vector<int> GetMechanismIndices(
    base::span<const AuthenticatorRequestDialogModel::Mechanism> mechanisms) {
  std::vector<int> tag_list(mechanisms.size());
  for (size_t i = 0; i < mechanisms.size(); i++) {
    tag_list[i] = static_cast<int>(i);
  }
  return tag_list;
}

}  // namespace

TransportHoverListModel::TransportHoverListModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : TransportHoverListModel(dialog_model,
                              GetMechanismIndices(dialog_model->mechanisms)) {}

TransportHoverListModel::TransportHoverListModel(
    AuthenticatorRequestDialogModel* dialog_model,
    std::vector<int> mechanism_indices_to_display)
    : mechanism_indices_to_display_(std::move(mechanism_indices_to_display)) {
  dialog_model_observation_.Observe(dialog_model);
}

TransportHoverListModel::~TransportHoverListModel() = default;

std::vector<int> TransportHoverListModel::GetButtonTags() const {
  return mechanism_indices_to_display_;
}

std::u16string TransportHoverListModel::GetItemText(int item_tag) const {
  return dialog_model_observation_.GetSource()->mechanisms[item_tag].name;
}

std::u16string TransportHoverListModel::GetDescriptionText(int item_tag) const {
  return dialog_model_observation_.GetSource()
      ->mechanisms[item_tag]
      .description;
}

ui::ImageModel TransportHoverListModel::GetItemIcon(int item_tag) const {
  return ui::ImageModel::FromVectorIcon(
      *dialog_model_observation_.GetSource()->mechanisms[item_tag].icon,
      IsButtonEnabled(item_tag) ? ui::kColorIcon : ui::kColorIconDisabled, 20);
}

bool TransportHoverListModel::IsButtonEnabled(int item_tag) const {
  return !dialog_model_observation_.GetSource()->ui_disabled_;
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  const auto& mech =
      dialog_model_observation_.GetSource()->mechanisms[item_tag];
  webauthn::user_actions::RecordMechanismClick(mech);
  mech.callback.Run();
}

size_t TransportHoverListModel::GetPreferredItemCount() const {
  return mechanism_indices_to_display_.size();
}

void TransportHoverListModel::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  dialog_model_observation_.Reset();
}

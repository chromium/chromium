// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

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
    base::span<const AuthenticatorRequestDialogModel::Mechanism> mechanisms)
    : TransportHoverListModel(mechanisms, GetMechanismIndices(mechanisms)) {}

TransportHoverListModel::TransportHoverListModel(
    base::span<const AuthenticatorRequestDialogModel::Mechanism> mechanisms,
    std::vector<int> mechanism_indices_to_display)
    : mechanisms_(mechanisms),
      mechanism_indices_to_display_(std::move(mechanism_indices_to_display)) {}

TransportHoverListModel::~TransportHoverListModel() = default;

std::vector<int> TransportHoverListModel::GetButtonTags() const {
  return mechanism_indices_to_display_;
}

std::u16string TransportHoverListModel::GetItemText(int item_tag) const {
  return mechanisms_[item_tag].name;
}

std::u16string TransportHoverListModel::GetDescriptionText(int item_tag) const {
  return mechanisms_[item_tag].description;
}

ui::ImageModel TransportHoverListModel::GetItemIcon(int item_tag) const {
  return ui::ImageModel::FromVectorIcon(*mechanisms_[item_tag].icon,
                                        ui::kColorIcon, 20);
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  mechanisms_[item_tag].callback.Run();
}

size_t TransportHoverListModel::GetPreferredItemCount() const {
  return mechanism_indices_to_display_.size();
}

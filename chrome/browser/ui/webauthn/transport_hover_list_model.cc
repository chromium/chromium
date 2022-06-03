// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

#include "ui/gfx/paint_vector_icon.h"

TransportHoverListModel::TransportHoverListModel(
    base::span<const AuthenticatorRequestDialogModel::Mechanism> mechanisms)
    : mechanisms_(mechanisms) {}

TransportHoverListModel::~TransportHoverListModel() = default;

bool TransportHoverListModel::ShouldShowPlaceholderForEmptyList() const {
  return false;
}

std::u16string TransportHoverListModel::GetPlaceholderText() const {
  return std::u16string();
}

const gfx::VectorIcon* TransportHoverListModel::GetPlaceholderIcon() const {
  return &gfx::kNoneIcon;
}

std::vector<int> TransportHoverListModel::GetThrobberTags() const {
  return {};
}

std::vector<int> TransportHoverListModel::GetButtonTags() const {
  std::vector<int> tag_list(mechanisms_.size());
  for (size_t i = 0; i < mechanisms_.size(); i++) {
    tag_list[i] = static_cast<int>(i);
  }
  return tag_list;
}

std::u16string TransportHoverListModel::GetItemText(int item_tag) const {
  return mechanisms_[item_tag].name;
}

std::u16string TransportHoverListModel::GetDescriptionText(int item_tag) const {
  return std::u16string();
}

const gfx::VectorIcon* TransportHoverListModel::GetItemIcon(
    int item_tag) const {
  return mechanisms_[item_tag].icon;
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  mechanisms_[item_tag].callback.Run();
}

size_t TransportHoverListModel::GetPreferredItemCount() const {
  return mechanisms_.size();
}

bool TransportHoverListModel::StyleForTwoLines() const {
  return false;
}

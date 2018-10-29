// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

#include <utility>

#include "chrome/browser/ui/webauthn/transport_utils.h"
#include "ui/gfx/paint_vector_icon.h"

TransportHoverListModel::TransportHoverListModel(
    std::vector<AuthenticatorTransport> transport_list,
    Delegate* delegate)
    : transport_list_(std::move(transport_list)), delegate_(delegate) {}

TransportHoverListModel::~TransportHoverListModel() = default;

bool TransportHoverListModel::ShouldShowPlaceholderForEmptyList() const {
  return false;
}

base::string16 TransportHoverListModel::GetPlaceholderText() const {
  return base::string16();
}

const gfx::VectorIcon& TransportHoverListModel::GetPlaceholderIcon() const {
  return gfx::kNoneIcon;
}

std::vector<int> TransportHoverListModel::GetItemTags() const {
  std::vector<int> tag_list(transport_list_.size());
  std::transform(
      transport_list_.begin(), transport_list_.end(), tag_list.begin(),
      [](const auto& transport) { return base::strict_cast<int>(transport); });
  return tag_list;
}

base::string16 TransportHoverListModel::GetItemText(int item_tag) const {
  return GetTransportHumanReadableName(
      static_cast<AuthenticatorTransport>(item_tag),
      TransportSelectionContext::kTransportSelectionSheet);
}

const gfx::VectorIcon& TransportHoverListModel::GetItemIcon(
    int item_tag) const {
  return GetTransportVectorIcon(static_cast<AuthenticatorTransport>(item_tag));
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  if (delegate_)
    delegate_->OnItemSelected(static_cast<AuthenticatorTransport>(item_tag));
}

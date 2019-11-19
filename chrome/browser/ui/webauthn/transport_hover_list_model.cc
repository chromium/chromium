// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/transport_utils.h"
#include "chrome/grit/generated_resources.h"
#include "device/fido/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {
// The tag ID space consists of the union of |AuthenticatorTransport| values
// with the extra values defined below. These extra values must not overlap with
// any values of |AuthenticatorTransport|.

constexpr int kTagExtraBase = 1 << 16;
// Command ID for triggering QR flow.
constexpr int kPairPhoneTag = kTagExtraBase + 0;
}  // namespace

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

const gfx::VectorIcon* TransportHoverListModel::GetPlaceholderIcon() const {
  return &gfx::kNoneIcon;
}

std::vector<int> TransportHoverListModel::GetItemTags() const {
  std::vector<int> tag_list(transport_list_.size());
  std::transform(
      transport_list_.begin(), transport_list_.end(), tag_list.begin(),
      [](const auto& transport) { return base::strict_cast<int>(transport); });

  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    tag_list.push_back(kPairPhoneTag);
  }
  return tag_list;
}

base::string16 TransportHoverListModel::GetItemText(int item_tag) const {
  if (item_tag == kPairPhoneTag) {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_POPUP_PAIR_PHONE);
  }

  return GetTransportHumanReadableName(
      static_cast<AuthenticatorTransport>(item_tag),
      TransportSelectionContext::kTransportSelectionSheet);
}

base::string16 TransportHoverListModel::GetDescriptionText(int item_tag) const {
  return base::string16();
}

const gfx::VectorIcon* TransportHoverListModel::GetItemIcon(
    int item_tag) const {
  if (item_tag == kPairPhoneTag) {
    return &kSmartphoneIcon;
  }
  return GetTransportVectorIcon(static_cast<AuthenticatorTransport>(item_tag));
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  if (!delegate_) {
    return;
  }

  if (item_tag == kPairPhoneTag) {
    delegate_->StartPhonePairing();
    return;
  }

  delegate_->OnTransportSelected(static_cast<AuthenticatorTransport>(item_tag));
}

size_t TransportHoverListModel::GetPreferredItemCount() const {
  return transport_list_.size() + static_cast<int>(base::FeatureList::IsEnabled(
                                      device::kWebAuthPhoneSupport));
}

bool TransportHoverListModel::StyleForTwoLines() const {
  return false;
}

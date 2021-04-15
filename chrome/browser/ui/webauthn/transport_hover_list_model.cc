// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/transport_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

namespace {
// The tag ID space consists of the union of |AuthenticatorTransport| values
// with the extra values defined below. These extra values must not overlap with
// any values of |AuthenticatorTransport|.

constexpr int kTagExtraBase = 1 << 16;
constexpr int kNativeWinApiTag = kTagExtraBase;

// kTagPhoneBase is the starting tag number for the names of linked phones.
constexpr int kTagPhoneBase = 1 << 20;

}  // namespace

TransportHoverListModel::TransportHoverListModel(
    base::flat_set<AuthenticatorTransport> transport_list,
    bool show_win_native_api_item,
    std::vector<std::string> phone_names,
    Delegate* delegate)
    : transport_list_(std::move(transport_list)),
      show_win_native_api_item_(show_win_native_api_item),
      phone_names_(std::move(phone_names)),
      delegate_(delegate) {}

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
  std::vector<int> tag_list(transport_list_.size());
  std::transform(
      transport_list_.begin(), transport_list_.end(), tag_list.begin(),
      [](const auto& transport) { return base::strict_cast<int>(transport); });
  if (show_win_native_api_item_) {
    tag_list.push_back(kNativeWinApiTag);
  }
  for (size_t i = 0; i < phone_names_.size(); i++) {
    tag_list.push_back(kTagPhoneBase + static_cast<int>(i));
  }

  return tag_list;
}

std::u16string TransportHoverListModel::GetItemText(int item_tag) const {
  if (item_tag >= kTagPhoneBase) {
    static constexpr size_t kMaxNameChars = 50;
    const std::u16string name =
        base::UTF8ToUTF16(phone_names_[item_tag - kTagPhoneBase]);
    std::u16string elided;
    gfx::ElideString(name, kMaxNameChars, &elided);
    return elided;
  }
  if (item_tag == kNativeWinApiTag) {
    return l10n_util::GetStringUTF16(
        IDS_WEBAUTHN_TRANSPORT_POPUP_DIFFERENT_AUTHENTICATOR_WIN);
  }
  return GetTransportHumanReadableName(
      static_cast<AuthenticatorTransport>(item_tag),
      TransportSelectionContext::kTransportSelectionSheet);
}

std::u16string TransportHoverListModel::GetDescriptionText(int item_tag) const {
  return std::u16string();
}

const gfx::VectorIcon* TransportHoverListModel::GetItemIcon(
    int item_tag) const {
  if (item_tag >= kTagPhoneBase) {
    return &kSmartphoneIcon;
  }
  if (item_tag == kNativeWinApiTag) {
    return GetTransportVectorIcon(
        AuthenticatorTransport::kUsbHumanInterfaceDevice);
  }

  return GetTransportVectorIcon(static_cast<AuthenticatorTransport>(item_tag));
}

void TransportHoverListModel::OnListItemSelected(int item_tag) {
  if (!delegate_) {
    return;
  }

  if (item_tag >= kTagPhoneBase) {
    delegate_->ContactPhone(phone_names_[item_tag - kTagPhoneBase]);
  } else if (item_tag == kNativeWinApiTag) {
    delegate_->StartWinNativeApi();
  } else {
    delegate_->OnTransportSelected(
        static_cast<AuthenticatorTransport>(item_tag));
  }
}

size_t TransportHoverListModel::GetPreferredItemCount() const {
  return transport_list_.size() +
         static_cast<size_t>(show_win_native_api_item_) + phone_names_.size();
}

bool TransportHoverListModel::StyleForTwoLines() const {
  return false;
}

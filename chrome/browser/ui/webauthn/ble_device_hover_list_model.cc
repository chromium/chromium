// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ble_device_hover_list_model.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>

#include "chrome/browser/ui/webauthn/transport_utils.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr size_t kDefaultItemViewCount = 2;

std::map<int, std::string>::iterator FindElementByValue(
    std::map<int, std::string>* item_map,
    base::StringPiece value) {
  DCHECK(item_map);
  return std::find_if(item_map->begin(), item_map->end(),
                      [value](const auto& key_value_pair) {
                        return key_value_pair.second == value;
                      });
}

bool ShouldShowItemInView(const AuthenticatorReference& authenticator) {
  return authenticator.is_in_pairing_mode && !authenticator.is_paired &&
         authenticator.transport == AuthenticatorTransport::kBluetoothLowEnergy;
}

}  // namespace

BleDeviceHoverListModel::BleDeviceHoverListModel(
    ObservableAuthenticatorList* authenticator_list,
    Delegate* delegate)
    : authenticator_list_(authenticator_list), delegate_(delegate) {
  int tag_counter = 0;
  for (const auto& authenticator : authenticator_list_->authenticator_list()) {
    authenticator_tags_.emplace(++tag_counter, authenticator.authenticator_id);
  }

  authenticator_list_->SetObserver(this);
}

BleDeviceHoverListModel::~BleDeviceHoverListModel() {
  authenticator_list_->RemoveObserver();
}

const AuthenticatorReference* BleDeviceHoverListModel::GetAuthenticator(
    int tag) const {
  auto it = authenticator_tags_.find(tag);
  CHECK(it != authenticator_tags_.end());
  return authenticator_list_->GetAuthenticator(it->second);
}

bool BleDeviceHoverListModel::ShouldShowPlaceholderForEmptyList() const {
  return true;
}

base::string16 BleDeviceHoverListModel::GetPlaceholderText() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLE_DEVICE_SELECTION_SEARCHING_LABEL);
}

const gfx::VectorIcon* BleDeviceHoverListModel::GetPlaceholderIcon() const {
  return GetTransportVectorIcon(AuthenticatorTransport::kBluetoothLowEnergy);
}

base::string16 BleDeviceHoverListModel::GetItemText(int item_tag) const {
  return GetAuthenticator(item_tag)->authenticator_display_name;
}

base::string16 BleDeviceHoverListModel::GetDescriptionText(int item_tag) const {
  return base::string16();
}

const gfx::VectorIcon* BleDeviceHoverListModel::GetItemIcon(
    int item_tag) const {
  return GetTransportVectorIcon(AuthenticatorTransport::kBluetoothLowEnergy);
}

std::vector<int> BleDeviceHoverListModel::GetItemTags() const {
  std::vector<int> tag_list;
  tag_list.reserve(authenticator_tags_.size());

  for (const auto& item : authenticator_tags_) {
    const auto* authenticator =
        authenticator_list_->GetAuthenticator(item.second);
    CHECK(authenticator);
    if (ShouldShowItemInView(*authenticator)) {
      tag_list.emplace_back(item.first);
    }
  }

  return tag_list;
}

void BleDeviceHoverListModel::OnListItemSelected(int item_tag) {
  auto authenticator_item = authenticator_tags_.find(item_tag);
  CHECK(authenticator_item != authenticator_tags_.end());
  delegate_->OnItemSelected(authenticator_item->second);
}

size_t BleDeviceHoverListModel::GetPreferredItemCount() const {
  return kDefaultItemViewCount;
}

bool BleDeviceHoverListModel::StyleForTwoLines() const {
  return false;
}

void BleDeviceHoverListModel::OnAuthenticatorAdded(
    const AuthenticatorReference& authenticator) {
  auto item_tag = authenticator_tags_.empty()
                      ? 0
                      : (--authenticator_tags_.end())->first + 1;
  authenticator_tags_.emplace(item_tag, authenticator.authenticator_id);

  if (ShouldShowItemInView(authenticator) && observer())
    observer()->OnListItemAdded(item_tag);
}

void BleDeviceHoverListModel::OnAuthenticatorRemoved(
    const AuthenticatorReference& removed_authenticator) {
  const auto& authenticator_id = removed_authenticator.authenticator_id;
  auto it = FindElementByValue(&authenticator_tags_, authenticator_id);
  CHECK(it != authenticator_tags_.end());
  const auto item_tag = it->first;
  authenticator_tags_.erase(it);
  if (observer() && ShouldShowItemInView(removed_authenticator))
    observer()->OnListItemRemoved(item_tag);
}

void BleDeviceHoverListModel::OnAuthenticatorPairingModeChanged(
    const AuthenticatorReference& changed_authenticator) {
  if (!observer())
    return;

  auto it = FindElementByValue(&authenticator_tags_,
                               changed_authenticator.authenticator_id);
  CHECK(it != authenticator_tags_.end());
  const auto changed_item_tag = it->first;
  if (ShouldShowItemInView(changed_authenticator)) {
    observer()->OnListItemChanged(changed_item_tag,
                                  ListItemChangeType::kAddToViewComponent);
  } else {
    observer()->OnListItemChanged(changed_item_tag,
                                  ListItemChangeType::kRemoveFromViewComponent);
  }
}

void BleDeviceHoverListModel::OnAuthenticatorIdChanged(
    const AuthenticatorReference& changed_authenticator,
    base::StringPiece previous_id) {
  auto it = FindElementByValue(&authenticator_tags_, previous_id);
  CHECK(it != authenticator_tags_.end());
  it->second = changed_authenticator.authenticator_id;
}

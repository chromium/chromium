// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/account_hover_list_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

AccountHoverListModel::AccountHoverListModel(
    const std::vector<device::AuthenticatorGetAssertionResponse>* response_list,
    Delegate* delegate)
    : response_list_(response_list), delegate_(delegate) {}

AccountHoverListModel::~AccountHoverListModel() = default;

bool AccountHoverListModel::ShouldShowPlaceholderForEmptyList() const {
  return false;
}

base::string16 AccountHoverListModel::GetPlaceholderText() const {
  return base::string16();
}

const gfx::VectorIcon* AccountHoverListModel::GetPlaceholderIcon() const {
  return &kUserAccountAvatarIcon;
}

std::vector<int> AccountHoverListModel::GetItemTags() const {
  std::vector<int> tag_list(response_list_->size());
  for (size_t i = 0; i < response_list_->size(); ++i)
    tag_list[i] = i;
  return tag_list;
}

base::string16 AccountHoverListModel::GetItemText(int item_tag) const {
  auto user = (*response_list_)[item_tag].user_entity();
  if (user->display_name && !user->display_name->empty())
    return base::UTF8ToUTF16(user->display_name.value());
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UNKNOWN_ACCOUNT);
}

base::string16 AccountHoverListModel::GetDescriptionText(int item_tag) const {
  auto user = (*response_list_)[item_tag].user_entity();
  return base::UTF8ToUTF16(user->name.value_or(""));
}

const gfx::VectorIcon* AccountHoverListModel::GetItemIcon(int item_tag) const {
  return nullptr;
}

void AccountHoverListModel::OnListItemSelected(int item_tag) {
  delegate_->OnItemSelected(item_tag);
}

size_t AccountHoverListModel::GetPreferredItemCount() const {
  return response_list_->size();
}

bool AccountHoverListModel::StyleForTwoLines() const {
  return true;
}

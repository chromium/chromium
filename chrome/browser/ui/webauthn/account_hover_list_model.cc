// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/account_hover_list_model.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"

constexpr size_t kIconSize = 20;

namespace {
std::u16string NameTokenForDisplay(std::string_view name_token) {
  if (name_token.empty()) {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UNKNOWN_ACCOUNT);
  }
  return base::UTF8ToUTF16(name_token);
}
}  // namespace

AccountHoverListModel::AccountHoverListModel(
    AuthenticatorRequestDialogModel* dialog_model,
    Delegate* delegate)
    : delegate_(delegate) {
  for (const device::DiscoverableCredentialMetadata& cred :
       dialog_model->creds) {
    items_.emplace_back(
        NameTokenForDisplay(cred.user.name.value_or("")), u"",
        ui::ImageModel::FromVectorIcon(vector_icons::kPasskeyIcon,
                                       dialog_model->ui_disabled_
                                           ? kColorWebAuthnIconColorDisabled
                                           : kColorWebAuthnIconColor,
                                       kIconSize),
        !dialog_model->ui_disabled_);
  }
}

AccountHoverListModel::~AccountHoverListModel() = default;

std::vector<int> AccountHoverListModel::GetButtonTags() const {
  std::vector<int> tag_list(items_.size());
  for (size_t i = 0; i < items_.size(); ++i) {
    tag_list[i] = i;
  }
  return tag_list;
}

std::u16string AccountHoverListModel::GetItemText(int item_tag) const {
  return items_.at(item_tag).text;
}

std::u16string AccountHoverListModel::GetDescriptionText(int item_tag) const {
  return items_.at(item_tag).description;
}

ui::ImageModel AccountHoverListModel::GetItemIcon(int item_tag) const {
  return items_.at(item_tag).icon;
}

bool AccountHoverListModel::IsButtonEnabled(int item_tag) const {
  return items_.at(item_tag).enabled;
}

void AccountHoverListModel::OnListItemSelected(int item_tag) {
  delegate_->CredentialSelected(item_tag);
}

size_t AccountHoverListModel::GetPreferredItemCount() const {
  return items_.size();
}

AccountHoverListModel::Item::Item(std::u16string text,
                                  std::u16string description,
                                  ui::ImageModel icon,
                                  bool enabled)
    : text(std::move(text)),
      description(std::move(description)),
      icon(icon),
      enabled(enabled) {}
AccountHoverListModel::Item::Item(Item&&) = default;
AccountHoverListModel::Item& AccountHoverListModel::Item::operator=(Item&&) =
    default;
AccountHoverListModel::Item::~Item() = default;

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/chooser_controller.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

ChooserController::ChooserController(std::u16string title) : title_(title) {}

ChooserController::~ChooserController() = default;

std::u16string ChooserController::GetTitle() const {
  return title_;
}

void ChooserController::View::OnAdapterAuthorizationChanged(bool authorized) {
  NOTREACHED_IN_MIGRATION();
}

bool ChooserController::ShouldShowIconBeforeText() const {
  return false;
}

bool ChooserController::ShouldShowHelpButton() const {
  return true;
}

bool ChooserController::ShouldShowReScanButton() const {
  return false;
}

bool ChooserController::AllowMultipleSelection() const {
  return false;
}

bool ChooserController::ShouldShowSelectAllCheckbox() const {
  return false;
}

std::u16string ChooserController::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_CANCEL_BUTTON_TEXT);
}

std::u16string ChooserController::GetSelectAllCheckboxLabel() const {
  return std::u16string();
}

bool ChooserController::BothButtonsAlwaysEnabled() const {
  return false;
}

bool ChooserController::TableViewAlwaysDisabled() const {
  return false;
}

int ChooserController::GetSignalStrengthLevel(size_t index) const {
  return -1;
}

bool ChooserController::IsConnected(size_t index) const {
  return false;
}

bool ChooserController::IsPaired(size_t index) const {
  return false;
}

void ChooserController::RefreshOptions() {
  NOTREACHED_IN_MIGRATION();
}

void ChooserController::OpenAdapterOffHelpUrl() const {
  NOTREACHED_IN_MIGRATION();
}

void ChooserController::OpenPermissionPreferences() const {
  NOTREACHED_IN_MIGRATION();
}

bool ChooserController::ShouldShowAdapterOffView() const {
  return false;
}

int ChooserController::GetAdapterOffMessageId() const {
  NOTREACHED();
}

int ChooserController::GetTurnAdapterOnLinkTextMessageId() const {
  NOTREACHED();
}

bool ChooserController::ShouldShowAdapterUnauthorizedView() const {
  return false;
}

int ChooserController::GetBluetoothUnauthorizedMessageId() const {
  NOTREACHED();
}

int ChooserController::GetAuthorizeBluetoothLinkTextMessageId() const {
  NOTREACHED();
}

}  // namespace permissions
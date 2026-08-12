// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_types.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace critical_actions {

CriticalActionEntry::CriticalActionEntry() = default;

CriticalActionEntry::CriticalActionEntry(const CriticalActionEntry&) = default;

CriticalActionEntry::CriticalActionEntry(CriticalActionEntry&&) noexcept =
    default;

CriticalActionEntry& CriticalActionEntry::operator=(
    const CriticalActionEntry&) = default;

CriticalActionEntry& CriticalActionEntry::operator=(
    CriticalActionEntry&&) noexcept = default;

CriticalActionEntry::~CriticalActionEntry() = default;

std::string CriticalActionEntry::GetLabel() const {
  switch (action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_PASSWORD_FILLED);
    case ActionType::kFederatedLogin:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_FEDERATED_LOGIN);
    case ActionType::kCredentialsOtp:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_CREDENTIALS_OTP);
    case ActionType::kFormFill:
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_FILLED);
    case ActionType::kDownload:
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD);
    case ActionType::kSettingChange:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_SETTING_CHANGE);
    case ActionType::kUnknown:
      return "";
  }
}

std::string CriticalActionEntry::GetTooltip() const {
  switch (action_type) {
    case ActionType::kCredentialAccess:
    case ActionType::kGooglePasswordManager:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_PASSWORD_TOOLTIP);
    case ActionType::kFederatedLogin:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_FEDERATED_LOGIN_TOOLTIP);
    case ActionType::kCredentialsOtp:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_CREDENTIALS_OTP_TOOLTIP);
    case ActionType::kFormFill:
      return l10n_util::GetStringUTF8(IDS_HISTORY_CRITICAL_ACTION_FORM_TOOLTIP);
    case ActionType::kDownload:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_DOWNLOAD_TOOLTIP);
    case ActionType::kSettingChange:
      return l10n_util::GetStringUTF8(
          IDS_HISTORY_CRITICAL_ACTION_SETTING_TOOLTIP);
    case ActionType::kUnknown:
      return "";
  }
}

CriticalActionQueryOptions::CriticalActionQueryOptions() = default;

CriticalActionQueryOptions::CriticalActionQueryOptions(
    const CriticalActionQueryOptions&) = default;

CriticalActionQueryOptions::CriticalActionQueryOptions(
    CriticalActionQueryOptions&&) noexcept = default;

CriticalActionQueryOptions& CriticalActionQueryOptions::operator=(
    const CriticalActionQueryOptions&) = default;

CriticalActionQueryOptions& CriticalActionQueryOptions::operator=(
    CriticalActionQueryOptions&&) noexcept = default;

CriticalActionQueryOptions::~CriticalActionQueryOptions() = default;

}  // namespace critical_actions

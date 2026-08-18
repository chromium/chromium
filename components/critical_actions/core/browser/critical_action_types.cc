// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_types.h"

#include "base/uuid.h"
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

CriticalActionEntry::Builder::Builder() {
  entry_.critical_action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry_.timestamp = base::Time::Now();
}

CriticalActionEntry::Builder::~Builder() = default;

CriticalActionEntry::Builder::Builder(Builder&&) noexcept = default;
CriticalActionEntry::Builder& CriticalActionEntry::Builder::operator=(
    Builder&&) noexcept = default;

CriticalActionEntry::Builder&&
CriticalActionEntry::Builder::SetCriticalActionId(
    std::string critical_action_id_val) && {
  entry_.critical_action_id = std::move(critical_action_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetTimestamp(
    base::Time timestamp_val) && {
  entry_.timestamp = timestamp_val;
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetActionType(
    ActionType action_type_val) && {
  entry_.action_type = action_type_val;
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetActionSource(
    ActionSource action_source_val) && {
  entry_.action_source = action_source_val;
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetUrl(
    GURL url_val) && {
  entry_.url = std::move(url_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetConversationId(
    std::string conversation_id_val) && {
  entry_.conversation_id = std::move(conversation_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetActorTaskId(
    std::string actor_task_id_val) && {
  entry_.actor_task_id = std::move(actor_task_id_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetMetadata(
    std::string metadata_val) && {
  entry_.metadata = std::move(metadata_val);
  return std::move(*this);
}

CriticalActionEntry::Builder&& CriticalActionEntry::Builder::SetVisitId(
    int64_t visit_id_val) && {
  entry_.visit_id = visit_id_val;
  return std::move(*this);
}

CriticalActionEntry CriticalActionEntry::Builder::Build() && {
  CHECK(!entry_.critical_action_id.empty());
  CHECK(!entry_.timestamp.is_null());
  CHECK(entry_.action_type != ActionType::kUnknown);
  CHECK(entry_.action_source != ActionSource::kUnknown);

  if (!entry_.url.is_valid()) {
    entry_.url = GURL();
  }

  return std::move(entry_);
}

}  // namespace critical_actions

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/critical_actions/critical_actions_page_handler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

namespace {

std::string ActionTypeToString(ActionType type) {
  switch (type) {
    case ActionType::kFormFill:
      return "FormFill";
    case ActionType::kDownload:
      return "Download";
    case ActionType::kSettingChange:
      return "SettingChange";
    case ActionType::kCredentialAccess:
      return "CredentialAccess";
    case ActionType::kGooglePasswordManager:
      return "GooglePasswordManager";
    case ActionType::kFederatedLogin:
      return "FederatedLogin";
    case ActionType::kCredentialsOtp:
      return "CredentialsOtp";
    case ActionType::kUnknown:
      return "Unknown";
  }
  return "Unknown";
}

std::string ActionSourceToString(ActionSource source) {
  switch (source) {
    case ActionSource::kPasswordManager:
      return "PasswordManager";
    case ActionSource::kActor:
      return "Actor";
    case ActionSource::kAutofill:
      return "Autofill";
    case ActionSource::kUnknown:
      return "Unknown";
  }
  return "Unknown";
}

bool MatchesSearchQuery(const CriticalActionEntry& entry,
                        const std::string& query_lower) {
  if (query_lower.empty()) {
    return true;
  }

  if (base::ToLowerASCII(entry.critical_action_id).find(query_lower) !=
      std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(entry.url.spec()).find(query_lower) !=
      std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(entry.conversation_id).find(query_lower) !=
      std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(entry.actor_task_id).find(query_lower) !=
      std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(entry.metadata).find(query_lower) !=
      std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(entry.GetLabel()).find(query_lower) !=
      std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(ActionTypeToString(entry.action_type))
          .find(query_lower) != std::string::npos) {
    return true;
  }
  if (base::ToLowerASCII(ActionSourceToString(entry.action_source))
          .find(query_lower) != std::string::npos) {
    return true;
  }
  if (base::NumberToString(entry.visit_id).find(query_lower) !=
      std::string::npos) {
    return true;
  }

  return false;
}

}  // namespace

CriticalActionsPageHandler::CriticalActionsPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  CHECK(profile_);
}

CriticalActionsPageHandler::~CriticalActionsPageHandler() = default;

void CriticalActionsPageHandler::GetCriticalActions(
    uint32_t page_index,
    uint32_t page_size,
    const std::optional<std::string>& search_query,
    std::optional<int32_t> action_type_filter,
    GetCriticalActionsCallback callback) {
  CriticalActionService* service =
      CriticalActionFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(mojom::CriticalActionsQueryResult::NewError(
        mojom::CriticalActionsError::kFeatureDisabled));
    return;
  }

  CriticalActionQueryOptions options;
  // Cap the database query to the most recent entries so that we never perform
  // an unbounded query or load excessive records into memory if the database
  // has grown very large. Because timestamp is indexed, SQLite resolves this
  // with a fast index scan.
  options.max_count = kMaxCriticalActionsQueryCount;
  if (action_type_filter.has_value() && action_type_filter.value() >= 0 &&
      action_type_filter.value() <=
          static_cast<int32_t>(ActionType::kMaxValue)) {
    options.action_types = {
        static_cast<ActionType>(action_type_filter.value())};
  }

  service->GetCriticalActions(
      options,
      base::BindOnce(&CriticalActionsPageHandler::OnGetCriticalActionsComplete,
                     weak_ptr_factory_.GetWeakPtr(), page_index, page_size,
                     search_query, action_type_filter, std::move(callback)));
}

void CriticalActionsPageHandler::OnGetCriticalActionsComplete(
    uint32_t page_index,
    uint32_t page_size,
    std::optional<std::string> search_query,
    std::optional<int32_t> action_type_filter,
    GetCriticalActionsCallback callback,
    std::vector<CriticalActionEntry> entries) {
  std::string query_lower =
      search_query.has_value() ? base::ToLowerASCII(*search_query) : "";

  std::vector<CriticalActionEntry> filtered;
  for (const auto& entry : entries) {
    if (action_type_filter.has_value() &&
        static_cast<int32_t>(entry.action_type) != *action_type_filter) {
      continue;
    }
    if (MatchesSearchQuery(entry, query_lower)) {
      filtered.push_back(entry);
    }
  }

  uint32_t total_entries = static_cast<uint32_t>(filtered.size());
  uint32_t effective_page_size = (page_size == 0) ? 25 : page_size;
  uint32_t total_pages =
      total_entries == 0
          ? 1
          : (total_entries + effective_page_size - 1) / effective_page_size;
  page_index = std::min(page_index, total_pages - 1);
  uint32_t start_idx = page_index * effective_page_size;

  auto list = mojom::CriticalActionsList::New();
  list->total_entries = total_entries;
  list->page_index = page_index;
  list->page_size = effective_page_size;

  // Extract the requested page slice [start_idx, end_idx) from the filtered
  // results and convert only those entries to Mojo items, avoiding transferring
  // the entire unpaginated list across IPC.
  if (start_idx < total_entries) {
    uint32_t end_idx = std::min(start_idx + effective_page_size, total_entries);
    for (uint32_t i = start_idx; i < end_idx; ++i) {
      const auto& entry = filtered[i];
      auto item = mojom::CriticalActionItem::New();
      item->critical_action_id = entry.critical_action_id;
      item->timestamp_raw =
          entry.timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds();
      item->timestamp_str = base::UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(entry.timestamp));
      item->visit_id = entry.visit_id;
      item->conversation_id = entry.conversation_id;
      item->actor_task_id = entry.actor_task_id;
      item->action_type = static_cast<int32_t>(entry.action_type);
      item->action_type_str = ActionTypeToString(entry.action_type);
      item->action_source = static_cast<int32_t>(entry.action_source);
      item->action_source_str = ActionSourceToString(entry.action_source);
      item->label = entry.GetLabel();
      item->tooltip = entry.GetTooltip();
      item->url = entry.url.spec();
      item->metadata = entry.metadata;
      list->entries.push_back(std::move(item));
    }
  }

  std::move(callback).Run(
      mojom::CriticalActionsQueryResult::NewList(std::move(list)));
}

void CriticalActionsPageHandler::DeleteCriticalAction(
    const std::string& critical_action_id,
    DeleteCriticalActionCallback callback) {
  CriticalActionService* service =
      CriticalActionFactory::GetForProfile(profile_);
  if (service) {
    service->DeleteCriticalAction(critical_action_id);
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void CriticalActionsPageHandler::ClearAllCriticalActions(
    ClearAllCriticalActionsCallback callback) {
  CriticalActionService* service =
      CriticalActionFactory::GetForProfile(profile_);
  if (service) {
    service->DeleteCriticalActionsInTimeRange(base::Time::Min(),
                                              base::Time::Max());
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace critical_actions

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_page_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace multistep_filter_internals {

namespace {

std::string_view LogEventTypeToString(multistep_filter::LogEventType type) {
  switch (type) {
    case multistep_filter::LogEventType::kNavigationStarted:
      return "Navigation Started";
    case multistep_filter::LogEventType::kUrlEligibilityCheck:
      return "Url Eligibility Check";
    case multistep_filter::LogEventType::kAnnotationExtractionStarted:
      return "Annotation Extraction Started";
    case multistep_filter::LogEventType::kAnnotationsExtracted:
      return "Annotations Extracted";
    case multistep_filter::LogEventType::kSuggestionGenerationStarted:
      return "Suggestion Generation Started";
    case multistep_filter::LogEventType::kNoSupportedTasks:
      return "No Supported Tasks";
    case multistep_filter::LogEventType::kNoRelevantAnnotations:
      return "No Relevant Annotations";
    case multistep_filter::LogEventType::kServerRequestSent:
      return "Server Request Sent";
    case multistep_filter::LogEventType::kServerResponseReceived:
      return "Server Response Received";
    case multistep_filter::LogEventType::kSuggestionGenerated:
      return "Suggestion Generated";
    case multistep_filter::LogEventType::kSuggestionSuppressed:
      return "Suggestion Suppressed";
    case multistep_filter::LogEventType::kSuggestionCleared:
      return "Suggestion Cleared";
    case multistep_filter::LogEventType::kSuggestionPreserved:
      return "Suggestion Preserved";
    case multistep_filter::LogEventType::kSuggestionShown:
      return "Suggestion Shown";
    case multistep_filter::LogEventType::kSuggestionAccepted:
      return "Suggestion Accepted";
    case multistep_filter::LogEventType::kSuggestionApplied:
      return "Suggestion Applied";
    case multistep_filter::LogEventType::kSuggestionDismissed:
      return "Suggestion Dismissed";
    case multistep_filter::LogEventType::kSuggestionIgnored:
      return "Suggestion Ignored";
    case multistep_filter::LogEventType::kServerRequestFailed:
      return "Server Request Failed";
    case multistep_filter::LogEventType::kServerResponseMalformed:
      return "Server Response Malformed";
  }
  NOTREACHED();
}

std::string ConvertDetailsToString(const base::DictValue& dict) {
  std::string result;
  for (auto [key, value] : dict) {
    if (!result.empty()) {
      result += ", ";
    }
    if (value.is_string()) {
      base::StrAppend(&result, {key, ": ", value.GetString()});
    } else if (value.is_bool()) {
      base::StrAppend(&result, {key, ": ", value.GetBool() ? "true" : "false"});
    } else if (value.is_int()) {
      base::StrAppend(&result,
                      {key, ": ", base::NumberToString(value.GetInt())});
    } else {
      base::StrAppend(&result, {key, ": (unsupported type)"});
    }
  }
  return result;
}

mojom::LogEntryPtr ConvertToMojo(const multistep_filter::LogEntry& entry) {
  mojom::LogEntryPtr mojo_entry = mojom::LogEntry::New();
  mojo_entry->timestamp = entry.timestamp;
  mojo_entry->event_type = LogEventTypeToString(entry.event_type);
  mojo_entry->host = entry.host;
  mojo_entry->navigation_id = entry.navigation_id;
  mojo_entry->details = ConvertDetailsToString(entry.details);
  return mojo_entry;
}

constexpr char kUnknown[] = "Unknown";

const char* OptInStateToString(
    optimization_guide::prefs::FeatureOptInState state) {
  switch (state) {
    case optimization_guide::prefs::FeatureOptInState::kNotInitialized:
      return "Not Initialized";
    case optimization_guide::prefs::FeatureOptInState::kEnabled:
      return "Enabled";
    case optimization_guide::prefs::FeatureOptInState::kDisabled:
      return "Disabled";
  }
  return kUnknown;
}

const char* PolicyStateToString(
    multistep_filter::SuggestionsPolicyState state) {
  switch (state) {
    case multistep_filter::SuggestionsPolicyState::kEnabled:
      return "Enabled (0)";
    case multistep_filter::SuggestionsPolicyState::kDisabled:
      return "Disabled (1)";
  }
  return kUnknown;
}

}  // namespace

MultistepFilterInternalsPageHandler::MultistepFilterInternalsPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page,
    Profile* profile,
    multistep_filter::MultistepFilterLogRouter* log_router)
    : profile_(profile),
      log_router_(log_router),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  if (log_router_) {
    log_router_observation_.Observe(log_router_);
  }
}

MultistepFilterInternalsPageHandler::~MultistepFilterInternalsPageHandler() =
    default;

void MultistepFilterInternalsPageHandler::GetBufferedLogs(
    GetBufferedLogsCallback callback) {
  if (!log_router_) {
    std::move(callback).Run({});
    return;
  }
  std::vector<multistep_filter::LogEntry> buffered_logs =
      log_router_->GetBufferedLogs();
  std::vector<mojom::LogEntryPtr> mojo_logs;
  mojo_logs.reserve(buffered_logs.size());
  for (const multistep_filter::LogEntry& entry : buffered_logs) {
    mojo_logs.push_back(ConvertToMojo(entry));
  }
  std::move(callback).Run(std::move(mojo_logs));
}

void MultistepFilterInternalsPageHandler::GetDebugInfo(
    GetDebugInfoCallback callback) {
  mojom::DebugInfoPtr info = mojom::DebugInfo::New();
  info->account_status = mojom::AccountStatus::New();
  info->consent_status = mojom::ConsentStatus::New();
  info->settings_status = mojom::SettingsStatus::New();

  multistep_filter::MultistepFilterService* service =
      multistep_filter::MultistepFilterServiceFactory::GetForProfile(profile_);

  if (service) {
    multistep_filter::AccountState account = service->GetAccountState();
    multistep_filter::ConsentState consent = service->GetConsentState();
    multistep_filter::SettingsState settings = service->GetSettingsState();

    info->account_status->is_signed_in = account.is_signed_in;
    info->account_status->can_use_model_execution_features =
        account.can_use_model_execution_features;

    info->consent_status->is_msbb_enabled = consent.is_msbb_enabled;
    info->consent_status->is_history_sync_enabled =
        consent.is_history_sync_enabled;

    info->settings_status->contextual_cueing_opt_in_state =
        OptInStateToString(settings.opt_in_state);
    info->settings_status->chrome_suggestions_policy_state =
        PolicyStateToString(settings.policy_state);

    info->is_eligible = account.IsEligible() && consent.IsFullyConsented() &&
                        settings.IsSmartSuggestionsEnabled();
  } else {
    info->account_status->is_signed_in = false;
    info->account_status->can_use_model_execution_features = false;
    info->consent_status->is_msbb_enabled = false;
    info->consent_status->is_history_sync_enabled = false;
    info->settings_status->contextual_cueing_opt_in_state = kUnknown;
    info->settings_status->chrome_suggestions_policy_state = kUnknown;
    info->is_eligible = false;
  }

  auto add_flag = [&](const base::Feature& feature) {
    mojom::FeatureFlagStatusPtr flag_status = mojom::FeatureFlagStatus::New();
    flag_status->name = feature.name;
    flag_status->enabled = base::FeatureList::IsEnabled(feature);
    info->feature_flags.push_back(std::move(flag_status));
  };

  add_flag(multistep_filter::kMultistepFilter);
  add_flag(multistep_filter::kMultistepFilterSendFeedback);
  add_flag(contextual_cueing::kContextualCueingV2);

  std::move(callback).Run(std::move(info));
}

void MultistepFilterInternalsPageHandler::OnLogEntryAdded(
    const multistep_filter::LogEntry& entry) {
  page_->OnLogEntryAdded(ConvertToMojo(entry));
}

void MultistepFilterInternalsPageHandler::OnLogRouterShutdown() {
  log_router_observation_.Reset();
  log_router_ = nullptr;
}

}  // namespace multistep_filter_internals

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/cross_device_tab_provider.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/omnibox/browser/actions/cross_device_tab_action.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/provider_state_service.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

namespace {

enum class MetricCategory {
  kMatch,
  kAction,
  kProvider,
};

std::string GetHistogramName(MetricCategory category, std::string_view suffix) {
  std::string_view infix;
  switch (category) {
    case MetricCategory::kMatch:
      infix = "Match.";
      break;
    case MetricCategory::kAction:
      infix = "Action.";
      break;
    case MetricCategory::kProvider:
      infix = "Provider.";
      break;
  }
  return base::StrCat({"Omnibox.CrossDeviceTab.", infix, suffix});
}

std::string GetInteractionHistogramName(bool is_action,
                                        std::string_view suffix) {
  return GetHistogramName(
      is_action ? MetricCategory::kAction : MetricCategory::kMatch, suffix);
}

// Creates a cross-device autocomplete match given a qualifying navigation.
AutocompleteMatch CreateCrossDeviceTabMatch(
    AutocompleteProvider* provider,
    const sessions::SerializedNavigationEntry& navigation,
    base::Time tab_last_active_time) {
  CHECK(provider);

  AutocompleteMatch match(provider, omnibox::kDefaultRemoteZeroSuggestRelevance,
                          /*deletable=*/false,
                          AutocompleteMatchType::CROSS_DEVICE_TAB);
  match.destination_url = navigation.virtual_url();
  match.description = navigation.title();
  match.description_class = ClassifyTermMatches(
      /*matches=*/{}, match.description.length(), ACMatchClassification::NONE,
      ACMatchClassification::NONE);

  // Zero suggest results should always omit protocols and never appear bold.
  const url_formatter::FormatUrlTypes format_types =
      AutocompleteMatch::GetFormatTypes(/*preserve_scheme=*/false,
                                        /*preserve_subdomain=*/false);

  match.contents = url_formatter::FormatUrl(
      match.destination_url, format_types, base::UnescapeRule::SPACES,
      /*new_parsed=*/nullptr, /*prefix_end=*/nullptr,
      /*offset_for_adjustment=*/nullptr);
  match.fill_into_edit = match.contents;
  match.contents_class = ClassifyTermMatches(
      /*matches=*/{}, match.contents.length(), ACMatchClassification::NONE,
      ACMatchClassification::URL);

  match.actions.push_back(
      base::MakeRefCounted<CrossDeviceTabAction>(tab_last_active_time));
  return match;
}

void LogMostRecentTabAge(base::TimeDelta age) {
  base::UmaHistogramCustomTimes(
      GetHistogramName(MetricCategory::kProvider, "MostRecentTabAge"), age,
      base::Minutes(1), base::Days(7), 50);
}

void LogFocusToOpenTime(bool is_action, base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(
      GetInteractionHistogramName(is_action, "FocusToOpenTime"), duration,
      base::Milliseconds(10), base::Minutes(3), 50);
}

void LogClickAge(bool is_action, base::TimeDelta age) {
  base::UmaHistogramCustomTimes(
      GetInteractionHistogramName(is_action, "ClickAge"), age, base::Minutes(1),
      base::Days(7), 50);
}

void LogShowAge(base::TimeDelta age) {
  base::UmaHistogramCustomTimes(
      GetInteractionHistogramName(/*is_action=*/false, "ShowAge"), age,
      base::Minutes(1), base::Days(7), 50);
}

}  // namespace

CrossDeviceTabProvider::CrossDeviceTabProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CROSS_DEVICE_TAB),
      client_(client) {}

CrossDeviceTabProvider::~CrossDeviceTabProvider() = default;

CrossDeviceTabProvider::QueryResult CrossDeviceTabProvider::GetMostRecentTab() {
  sync_sessions::SessionSyncService* service = client_->GetSessionSyncService();
  if (!service) {
    return base::unexpected(Eligibility::kNoSyncService);
  }

  sync_sessions::OpenTabsUIDelegate* delegate =
      service->GetOpenTabsUIDelegate();
  if (!delegate) {
    return base::unexpected(Eligibility::kNoOpenTabsDelegate);
  }

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  if (!delegate->GetAllForeignSessions(&sessions) || sessions.empty()) {
    return base::unexpected(Eligibility::kNoForeignSessions);
  }

  const sessions::SessionTab* most_recent_tab = nullptr;
  base::Time most_recent_time;

  for (const sync_sessions::SyncedSession* session : sessions) {
    for (const auto& [window_id, window] : session->windows) {
      for (const auto& tab : window->wrapped_window.tabs) {
        if (!tab->navigations.empty() && tab->timestamp > most_recent_time) {
          most_recent_time = tab->timestamp;
          most_recent_tab = tab.get();
        }
      }
    }
  }

  if (!most_recent_tab) {
    return base::unexpected(Eligibility::kNoTabs);
  }

  const sessions::SerializedNavigationEntry& most_recent_navigation =
      most_recent_tab->navigations.at(
          most_recent_tab->normalized_navigation_index());

  const GURL& url = most_recent_navigation.virtual_url();
  // TODO(crbug.com/508162292): Investigate if `ShouldSyncURL()` should be used
  //   here.
  if (!url.is_valid()) {
    return base::unexpected(Eligibility::kInvalidUrl);
  }

  return most_recent_tab;
}

void CrossDeviceTabProvider::LogEligibility(Eligibility eligibility) {
  base::UmaHistogramEnumeration(
      GetHistogramName(MetricCategory::kProvider, "Eligibility"), eligibility);
}

void CrossDeviceTabProvider::Start(const AutocompleteInput& input,
                                   bool minimal_changes) {
  matches_.clear();
  most_recent_tab_timestamp_ = base::Time();

  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxCrossDeviceTabZeroSuggest)) {
    return;
  }

  if (!input.IsZeroSuggest()) {
    return;
  }

  const QueryResult result = GetMostRecentTab();

  if (!result.has_value()) {
    LogEligibility(result.error());
    return;
  }

  const sessions::SessionTab* most_recent_tab = result.value();

  const base::TimeDelta age = base::Time::Now() - most_recent_tab->timestamp;
  LogMostRecentTabAge(age);

  if (!IsRecentEnoughRemoteTimestampToSuggestRemoteTab(
          most_recent_tab->timestamp)) {
    if (age >
        base::Minutes(
            omnibox::
                kOmniboxCrossDeviceTabZeroSuggestDelayedContinuationMaxAgeMinutes
                    .Get())) {
      LogEligibility(Eligibility::kTabTooOld);
    } else {
      LogEligibility(Eligibility::kLocalSessionNotRecent);
    }
    return;
  }

  const sessions::SerializedNavigationEntry& most_recent_navigation =
      most_recent_tab->navigations.at(
          most_recent_tab->normalized_navigation_index());

  matches_.push_back(CreateCrossDeviceTabMatch(
      /*provider=*/this, most_recent_navigation, most_recent_tab->timestamp));
  most_recent_tab_timestamp_ = most_recent_tab->timestamp;

  LogEligibility(Eligibility::kMatchCreated);
}

// static
void CrossDeviceTabProvider::RecordInteractionMetrics(const OmniboxLog& log) {
  bool cross_device_tab_shown = false;
  size_t cross_device_tab_position = 0;
  for (size_t i = 0; i < log.result->size(); ++i) {
    if (log.result->match_at(i).type ==
        AutocompleteMatchType::CROSS_DEVICE_TAB) {
      cross_device_tab_shown = true;
      cross_device_tab_position = i;
      break;
    }
  }

  if (!cross_device_tab_shown) {
    return;
  }

  const size_t position = cross_device_tab_position;

  const bool cross_device_row_clicked =
      log.result->match_at(log.selection.line).type ==
      AutocompleteMatchType::CROSS_DEVICE_TAB;

  // Log actual age when shown (impression) on navigation.
  const AutocompleteMatch& match = log.result->match_at(position);
  OmniboxAction* const shown_action =
      match.GetActionWhere([](const scoped_refptr<OmniboxAction>& a) {
        return a->ActionId() == OmniboxActionId::CROSS_DEVICE_TAB;
      });
  if (shown_action) {
    CrossDeviceTabAction* const cross_device_action =
        static_cast<CrossDeviceTabAction*>(shown_action);
    const base::Time tab_last_active_time =
        cross_device_action->tab_last_active_time();
    if (!tab_last_active_time.is_null()) {
      const base::TimeDelta tab_age = base::Time::Now() - tab_last_active_time;
      LogShowAge(tab_age);
    }
  }

  // Click-related metrics (time to click, age of tab when clicked).
  if (cross_device_row_clicked) {
    // Focus-to-open time has an invalid default value of -1ms in logs.
    const base::TimeDelta invalid_time_delta = base::Milliseconds(-1);
    if (log.elapsed_time_since_user_focused_omnibox != invalid_time_delta) {
      LogFocusToOpenTime(log.selection.IsAction(),
                         log.elapsed_time_since_user_focused_omnibox);
    }

    const AutocompleteMatch& clicked_match =
        log.result->match_at(log.selection.line);
    OmniboxAction* const clicked_action =
        clicked_match.GetActionWhere([](const scoped_refptr<OmniboxAction>& a) {
          return a->ActionId() == OmniboxActionId::CROSS_DEVICE_TAB;
        });
    if (clicked_action) {
      CrossDeviceTabAction* const cross_device_action =
          static_cast<CrossDeviceTabAction*>(clicked_action);
      const base::Time tab_last_active_time =
          cross_device_action->tab_last_active_time();
      if (!tab_last_active_time.is_null()) {
        const base::TimeDelta age = base::Time::Now() - tab_last_active_time;
        LogClickAge(log.selection.IsAction(), age);
      }
    }
  }
}

bool CrossDeviceTabProvider::IsVeryRecentRemoteTimestamp(
    base::Time timestamp) const {
  const base::TimeDelta age = base::Time::Now() - timestamp;
  return age <=
         base::Minutes(
             omnibox::kOmniboxCrossDeviceTabZeroSuggestMaxAgeMinutes.Get());
}

bool CrossDeviceTabProvider::
    IsModeratelyRecentRemoteTimestampWithRecentLocalSessionStart(
        base::Time timestamp) const {
  const base::TimeDelta age = base::Time::Now() - timestamp;

  ProviderStateService* provider_state_service =
      client_->GetProviderStateService();
  // `GetSessionSyncService()` being non-null guarantees `ProviderStateService`
  // is non-null too.
  CHECK(provider_state_service);

  // TODO(crbug.com/508162292): Revisit if a more sophisticated approach for
  //   "activation" could be implemented (e.g. time since last system resume).
  const base::TimeDelta profile_uptime =
      provider_state_service->profile_uptime_timer.Elapsed();

  return age <=
             base::Minutes(
                 omnibox::
                     kOmniboxCrossDeviceTabZeroSuggestDelayedContinuationMaxAgeMinutes
                         .Get()) &&
         profile_uptime <=
             base::Minutes(
                 omnibox::
                     kOmniboxCrossDeviceTabZeroSuggestMaxDelayedContinuationUptimeMinutes
                         .Get());
}

bool CrossDeviceTabProvider::IsRecentEnoughRemoteTimestampToSuggestRemoteTab(
    base::Time timestamp) const {
  return IsVeryRecentRemoteTimestamp(timestamp) ||
         IsModeratelyRecentRemoteTimestampWithRecentLocalSessionStart(
             timestamp);
}

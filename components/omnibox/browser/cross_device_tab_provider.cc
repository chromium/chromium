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
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/omnibox/browser/actions/cross_device_tab_action.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
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

// Returns the tab from other devices with the latest activity (with at least
// one navigation) or null if there is none.
const sessions::SessionTab* GetMostRecentTab(
    sync_sessions::SessionSyncService* service) {
  if (!service) {
    return nullptr;
  }

  sync_sessions::OpenTabsUIDelegate* delegate =
      service->GetOpenTabsUIDelegate();
  if (!delegate) {
    return nullptr;
  }

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  if (!delegate->GetAllForeignSessions(&sessions)) {
    return nullptr;
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

  return most_recent_tab;
}

// Creates a cross-device autocomplete match given a qualifying navigation.
AutocompleteMatch CreateCrossDeviceTabMatch(
    AutocompleteProvider* provider,
    const sessions::SerializedNavigationEntry& navigation) {
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
  match.contents_class = ClassifyTermMatches(
      /*matches=*/{}, match.contents.length(), ACMatchClassification::NONE,
      ACMatchClassification::URL);

  match.actions.push_back(base::MakeRefCounted<CrossDeviceTabAction>());
  return match;
}

}  // namespace

CrossDeviceTabProvider::CrossDeviceTabProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CROSS_DEVICE_TAB),
      client_(client) {}

CrossDeviceTabProvider::~CrossDeviceTabProvider() = default;

void CrossDeviceTabProvider::Start(const AutocompleteInput& input,
                                   bool minimal_changes) {
  matches_.clear();

  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxCrossDeviceTabZeroSuggest)) {
    return;
  }

  if (!input.IsZeroSuggest()) {
    return;
  }

  const sessions::SessionTab* most_recent_tab =
      GetMostRecentTab(client_->GetSessionSyncService());
  if (!most_recent_tab) {
    return;
  }

  const base::TimeDelta age = base::Time::Now() - most_recent_tab->timestamp;
  if (age >
      base::Minutes(omnibox::kOmniboxCrossDeviceTabZeroSuggestMaxAge.Get())) {
    return;
  }

  const sessions::SerializedNavigationEntry& most_recent_navigation =
      most_recent_tab->navigations.at(
          most_recent_tab->normalized_navigation_index());

  const GURL& url = most_recent_navigation.virtual_url();
  // TODO(crbug.com/508162292): Investigate if ShouldSyncURL() should be used
  // here.
  if (!url.is_valid()) {
    return;
  }

  matches_.push_back(
      CreateCrossDeviceTabMatch(/*provider=*/this, most_recent_navigation));
}

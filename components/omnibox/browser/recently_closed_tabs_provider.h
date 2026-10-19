// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_RECENTLY_CLOSED_TABS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_RECENTLY_CLOSED_TABS_PROVIDER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;
class GURL;

// Provides suggestions for tabs recently closed on this device, read from the
// TabRestoreService. Currently only enabled for the Android Tab Search overlay.
class RecentlyClosedTabsProvider : public AutocompleteProvider,
                                   public sessions::TabRestoreServiceObserver {
 public:
  RecentlyClosedTabsProvider(AutocompleteProviderClient* client,
                             AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;

  // sessions::TabRestoreServiceObserver:
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  ~RecentlyClosedTabsProvider() override;

  // Populates `matches_` from the TabRestoreService. If the service has not
  // finished loading, waits for it to load and notifies listeners once the
  // matches are available.
  void StartWithTabRestoreService(const AutocompleteInput& input);

  // Populates `matches_` with the recently closed tabs in `service` that match
  // `input`.
  void AddMatches(const AutocompleteInput& input,
                  const sessions::TabRestoreService& service);

  AutocompleteMatch CreateRecentlyClosedTabMatch(const AutocompleteInput& input,
                                                 const std::u16string& title,
                                                 const GURL& url,
                                                 int relevance);

  raw_ptr<AutocompleteProviderClient> client_;

  // The input to populate matches for once the TabRestoreService loads.
  std::optional<AutocompleteInput> pending_input_;

  base::ScopedObservation<sessions::TabRestoreService,
                          sessions::TabRestoreServiceObserver>
      tab_restore_service_observation_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_RECENTLY_CLOSED_TABS_PROVIDER_H_

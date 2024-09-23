// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RECENT_TABS_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RECENT_TABS_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"

class Profile;

namespace ash {

// Manages fetching foreign session tabs for the birch feature. Fetched tabs
// are sent to the `BirchModel` to be stored.
class ASH_EXPORT BirchRecentTabsProvider : public BirchDataProvider {
 public:
  explicit BirchRecentTabsProvider(Profile* profile);
  BirchRecentTabsProvider(const BirchRecentTabsProvider&) = delete;
  BirchRecentTabsProvider& operator=(const BirchRecentTabsProvider&) = delete;
  ~BirchRecentTabsProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  // Once subscribed, this is called when the session sync service notifies of
  // foreign sessions changed.
  void OnForeignSessionsChanged();

  void OnTabsRetrieved(std::vector<crosapi::mojom::TabSuggestionItemPtr> items);

 private:
  raw_ptr<Profile> profile_ = nullptr;

  base::CallbackListSubscription foreign_sessions_subscription_;

  base::WeakPtrFactory<BirchRecentTabsProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RECENT_TABS_PROVIDER_H_

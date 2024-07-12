// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_SELF_SHARE_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_SELF_SHARE_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "base/memory/weak_ptr.h"
#include "components/favicon_base/favicon_types.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

class Profile;

namespace ash {

// Manages fetching tabs shared to self via ChromeSync for the birch feature.
// Fetched tabs are sent to 'BirchModel' to be stored.
class ASH_EXPORT BirchSelfShareProvider : public BirchDataProvider {
 public:
  explicit BirchSelfShareProvider(Profile* profile);
  BirchSelfShareProvider(const BirchSelfShareProvider&) = delete;
  BirchSelfShareProvider& operator=(const BirchSelfShareProvider&) = delete;
  ~BirchSelfShareProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

 protected:
  // Marks the entry as opened when the suggestion is pressed.
  void OnItemPressed(const std::string& guid);

 private:
  friend class BirchKeyedServiceTest;

  const raw_ptr<Profile> profile_;

  const raw_ptr<send_tab_to_self::SendTabToSelfSyncService> sync_service_;

  // Cached self share items are used when no new changes have been detected
  // from ChromeSync.
  std::vector<BirchSelfShareItem> items_;

  base::WeakPtrFactory<BirchSelfShareProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_SELF_SHARE_PROVIDER_H_

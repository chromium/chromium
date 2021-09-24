// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/recent_app_click_observer.h"

namespace chromeos {
namespace phonehub {

// The handler that exposes APIs to interact with Phone Hub Recent Apps.
// TODO(paulzchen): Implement Eche's RecentAppClickObserver and add/remove
// observer via this handler.
class RecentAppsInteractionHandler {
 public:
  RecentAppsInteractionHandler();
  RecentAppsInteractionHandler(const RecentAppsInteractionHandler&) = delete;
  RecentAppsInteractionHandler* operator=(const RecentAppsInteractionHandler&) =
      delete;
  virtual ~RecentAppsInteractionHandler();

  virtual void NotifyRecentAppClicked(const std::string& package_name,
                                      const std::u16string& visible_name);
  virtual void AddRecentAppClickObserver(RecentAppClickObserver* observer);
  virtual void RemoveRecentAppClickObserver(RecentAppClickObserver* observer);

  virtual void NotifyRecentAppAddedOrUpdated(
      const Notification::AppMetadata& app_metadata,
      base::Time last_accessed_timestamp);
  virtual std::vector<Notification::AppMetadata> FetchRecentAppMetadataList();

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentAppsInteractionHandlerTest, RecentAppsUpdated);

  base::ObserverList<RecentAppClickObserver> observer_list_;
  std::vector<std::pair<Notification::AppMetadata, base::Time>>
      recent_app_metadata_list_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_H_

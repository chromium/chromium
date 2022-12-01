// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APP_CLICK_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APP_CLICK_OBSERVER_H_

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/notification.h"

namespace ash {
namespace phonehub {

// Interface used to listen for a recent app click.
class RecentAppClickObserver : public base::CheckedObserver {
 public:
  ~RecentAppClickObserver() override = default;
  // Called when the user clicks the recent app which has an open
  // action in the PhoneHub.
  virtual void OnRecentAppClicked(
      const Notification::AppMetadata& app_metadata,
      eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) = 0;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APP_CLICK_OBSERVER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_H_

#include <stdint.h>

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/components/phonehub/recent_app_click_observer.h"

namespace ash::phonehub {

// The handler that exposes APIs to interact with Phone Hub Recent Apps.
// TODO(paulzchen): Implement Eche's RecentAppClickObserver and add/remove
// observer via this handler.
class RecentAppsInteractionHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    // Notifies observers that recent apps view needs be refreshed, the access
    // state of recent apps feature is updated or current recent apps list has
    // changed.
    virtual void OnRecentAppsUiStateUpdated() = 0;
  };

  enum class RecentAppsUiState {
    // Feature is either not supported, or supported but disabled by user.
    HIDDEN,
    // Feature is supported and enabled but no recent app has been added yet.
    PLACEHOLDER_VIEW,
    // A bootstrap connection with the phone is being attempted.
    LOADING,
    // The bootstrap connection has failed.
    CONNECTION_FAILED,
    // We have recent app that can be displayed.
    ITEMS_VISIBLE,
  };

  // Records each users' quiet mode to decide showing recent apps or not.
  struct UserState {
    int64_t user_id;
    // The state when user turn on/off work apps/notification.
    bool is_enabled;
  };

  RecentAppsInteractionHandler(const RecentAppsInteractionHandler&) = delete;
  RecentAppsInteractionHandler& operator=(const RecentAppsInteractionHandler&) =
      delete;
  virtual ~RecentAppsInteractionHandler();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  RecentAppsUiState ui_state() { return ui_state_; }
  std::vector<UserState> user_states() { return user_states_; }
  void set_user_states(const std::vector<UserState>& user_states) {
    user_states_ = user_states;
  }

  virtual void AddRecentAppClickObserver(RecentAppClickObserver* observer);
  virtual void RemoveRecentAppClickObserver(RecentAppClickObserver* observer);

  virtual void SetConnectionStatusHandler(
      eche_app::EcheConnectionStatusHandler*
          eche_connection_status_handler) = 0;

  virtual void NotifyRecentAppClicked(
      const Notification::AppMetadata& app_metadata,
      eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) = 0;
  virtual void NotifyRecentAppAddedOrUpdated(
      const Notification::AppMetadata& app_metadata,
      base::Time last_accessed_timestamp) = 0;
  virtual std::vector<Notification::AppMetadata>
  FetchRecentAppMetadataList() = 0;
  virtual void SetStreamableApps(
      const std::vector<Notification::AppMetadata>& streamable_apps) = 0;
  virtual void RemoveStreamableApp(const proto::App streamable_app) = 0;

  void set_ui_state_for_testing(RecentAppsUiState ui_state) {
    ui_state_ = ui_state;
  }

 protected:
  RecentAppsInteractionHandler();

  RecentAppsUiState ui_state_ = RecentAppsUiState::HIDDEN;
  void NotifyRecentAppsViewUiStateUpdated();

 private:
  base::ObserverList<RecentAppClickObserver> recent_app_click_observer_list_;
  base::ObserverList<Observer> observer_list_;
  std::vector<UserState> user_states_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_H_

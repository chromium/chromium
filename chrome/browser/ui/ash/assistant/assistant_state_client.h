// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_STATE_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_STATE_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/user_manager/user_manager.h"

class PrefChangeRegistrar;

class AssistantStateClient
    : public user_manager::UserManager::UserSessionStateObserver,
      public arc::ArcSessionManagerObserver {
 public:
  AssistantStateClient();

  AssistantStateClient(const AssistantStateClient&) = delete;
  AssistantStateClient& operator=(const AssistantStateClient&) = delete;

  ~AssistantStateClient() override;

 private:
  friend class AssistantStateClientTest;

  // Notify the controller about state changes.
  void NotifyFeatureAllowed();
  void NotifyLocaleChanged();

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<AssistantStateClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_STATE_CLIENT_H_

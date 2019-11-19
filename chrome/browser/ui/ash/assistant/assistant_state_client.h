// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_STATE_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_STATE_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "components/user_manager/user_manager.h"

class PrefChangeRegistrar;

class AssistantStateClient
    : public user_manager::UserManager::UserSessionStateObserver,
      public arc::ArcSessionManager::Observer {
 public:
  AssistantStateClient();
  ~AssistantStateClient() override;

 private:
  friend class AssistantStateClientTest;

  // Notify the controller about state changes.
  void NotifyFeatureAllowed();
  void NotifyLocaleChanged();

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // arc::ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  Profile* profile_ = nullptr;

  base::WeakPtrFactory<AssistantStateClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantStateClient);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_STATE_CLIENT_H_

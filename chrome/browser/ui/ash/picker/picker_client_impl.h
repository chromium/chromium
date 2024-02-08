// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/picker/picker_client.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"

class Profile;

namespace app_list {
class SearchEngine;
}

namespace ash {
class PickerController;
}

namespace user_manager {
class User;
}

// Implements the PickerClient used by Ash.
class PickerClientImpl
    : public ash::PickerClient,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // Sets this instance as the client of `controller`.
  // Automatically unsets the client when this instance is destroyed.
  // `manager` needs to outlive this class.
  explicit PickerClientImpl(ash::PickerController* controller,
                            user_manager::UserManager* user_manager);
  PickerClientImpl(const PickerClientImpl&) = delete;
  PickerClientImpl& operator=(const PickerClientImpl&) = delete;
  ~PickerClientImpl() override;

  // ash::PickerClient:
  std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) override;
  void DownloadGifToString(const ash::ValidGifUrl& url,
                           DownloadGifToStringCallback callback) override;
  void StartCrosSearch(const std::u16string& query,
                       CrosSearchResultsCallback callback) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

 private:
  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  raw_ptr<ash::PickerController> controller_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  std::unique_ptr<app_list::SearchEngine> search_engine_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      user_session_state_observation_{this};

  base::WeakPtrFactory<PickerClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_

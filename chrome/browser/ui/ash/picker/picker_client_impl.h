// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_

#include "ash/public/cpp/picker/picker_client.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"

class Profile;

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
  explicit PickerClientImpl(ash::PickerController* controller);
  PickerClientImpl(const PickerClientImpl&) = delete;
  PickerClientImpl& operator=(const PickerClientImpl&) = delete;
  ~PickerClientImpl() override;

  // ash::PickerClient:
  std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) override;
  void DownloadGifToString(const GURL& url,
                           DownloadGifToStringCallback callback) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

 private:
  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  raw_ptr<ash::PickerController> controller_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<PickerClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/user_adding_screen.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_mojo.h"
#include "chrome/browser/ui/ash/login/user_adding_screen_input_methods_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

class UserAddingScreenImpl : public UserAddingScreen {
 public:
  void Start() override;
  void Cancel() override;
  bool IsRunning() override;

  void AddObserver(UserAddingScreen::Observer* observer) override;
  void RemoveObserver(UserAddingScreen::Observer* observer) override;

  static UserAddingScreenImpl* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UserAddingScreenImpl>;

  void OnDisplayHostCompletion();

  UserAddingScreenImpl();
  ~UserAddingScreenImpl() override;

  base::ObserverList<UserAddingScreen::Observer>::UncheckedAndDanglingUntriaged
      observers_;
  raw_ptr<LoginDisplayHost, DanglingUntriaged> display_host_;

  UserAddingScreenInputMethodsController im_controller_;
};

void UserAddingScreenImpl::Start() {
  CHECK(!IsRunning());
  display_host_ = new LoginDisplayHostMojo(DisplayedScreen::USER_ADDING_SCREEN);

  // This triggers input method manager to filter login screen methods. This
  // should happen before setting user input method, which happens when focusing
  // user pod (triggered by SetSessionState)"
  for (auto& observer : observers_) {
    observer.OnBeforeUserAddingScreenStarted();
  }

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  display_host_->StartUserAdding(base::BindOnce(
      &UserAddingScreenImpl::OnDisplayHostCompletion, base::Unretained(this)));
}

void UserAddingScreenImpl::Cancel() {
  CHECK(IsRunning());

  display_host_->CancelUserAdding();

  // Reset wallpaper if cancel adding user from multiple user sign in page.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    WallpaperControllerClientImpl::Get()->ShowUserWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  }
}

bool UserAddingScreenImpl::IsRunning() {
  return display_host_ != nullptr;
}

void UserAddingScreenImpl::AddObserver(UserAddingScreen::Observer* observer) {
  observers_.AddObserver(observer);
}

void UserAddingScreenImpl::RemoveObserver(
    UserAddingScreen::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserAddingScreenImpl::OnDisplayHostCompletion() {
  CHECK(IsRunning());
  display_host_ = nullptr;

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  for (auto& observer : observers_) {
    observer.OnUserAddingFinished();
  }
}

// static
UserAddingScreenImpl* UserAddingScreenImpl::GetInstance() {
  return base::Singleton<UserAddingScreenImpl>::get();
}

UserAddingScreenImpl::UserAddingScreenImpl()
    : display_host_(nullptr), im_controller_(this) {}

UserAddingScreenImpl::~UserAddingScreenImpl() {}

}  // anonymous namespace

UserAddingScreen::UserAddingScreen() {}
UserAddingScreen::~UserAddingScreen() {}

UserAddingScreen* UserAddingScreen::Get() {
  return UserAddingScreenImpl::GetInstance();
}

}  // namespace ash

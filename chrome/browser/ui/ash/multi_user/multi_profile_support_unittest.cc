// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string_view>
#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/multi_user/user_switch_animator.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/new_window/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace {

const char kAAccountIdString[] = R"({"account_type":"unknown","email":"a"})";
const char kBAccountIdString[] = R"({"account_type":"unknown","email":"b"})";
const char kArrowBAccountIdString[] =
    R"(->{"account_type":"unknown","email":"b"})";

const content::BrowserContext* GetActiveContext() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  return active_user ? multi_user_util::GetProfileFromAccountId(
                           active_user->GetAccountId())
                     : nullptr;
}

class TestShellDelegateChromeOS : public ash::TestShellDelegate {
 public:
  TestShellDelegateChromeOS() {}

  TestShellDelegateChromeOS(const TestShellDelegateChromeOS&) = delete;
  TestShellDelegateChromeOS& operator=(const TestShellDelegateChromeOS&) =
      delete;

  bool CanShowWindowForUser(const aura::Window* window) const override {
    return ::CanShowWindowForUser(window,
                                  base::BindRepeating(&GetActiveContext));
  }
};

std::unique_ptr<Browser> CreateTestBrowser(aura::Window* window,
                                           const gfx::Rect& bounds,
                                           Browser::CreateParams* params) {
  if (!bounds.IsEmpty())
    window->SetBounds(bounds);
  std::unique_ptr<Browser> browser =
      chrome::CreateBrowserWithAuraTestWindowForParams(base::WrapUnique(window),
                                                       params);
  return browser;
}

}  // namespace

namespace ash {

// A test class for preparing the MultiUserWindowManager. It creates
// various windows and instantiates the MultiUserWindowManager.
class MultiProfileSupportTest : public ChromeAshTestBase {
 public:
  MultiProfileSupportTest()
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())) {}

  MultiProfileSupportTest(const MultiProfileSupportTest&) = delete;
  MultiProfileSupportTest& operator=(const MultiProfileSupportTest&) = delete;

  // ChromeAshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  void SwitchActiveUser(const AccountId& id) {
    fake_user_manager_->SwitchActiveUser(id);
    ash::MultiUserWindowManagerImpl::Get()->OnActiveUserSessionChanged(id);
  }

  // Set up the test environment for this many windows.
  void SetUpForThisManyWindows(int windows);

  // If |windows_| is empty, set up one window each desk for a given user
  // without activating any desk and return a list of created widgets.
  // Otherwise, do nothing and return an empty vector.
  std::vector<std::unique_ptr<views::Widget>> SetUpOneWindowEachDeskForUser(
      AccountId account_id);

  // Switch the user and wait until the animation is finished.
  void SwitchUserAndWaitForAnimation(const AccountId& account_id) {
    EnsureTestUser(account_id);
    ash::MultiUserWindowManagerImpl::Get()->OnActiveUserSessionChanged(
        account_id);
    base::TimeTicks now = base::TimeTicks::Now();
    while (
        ash::MultiUserWindowManagerImpl::Get()->IsAnimationRunningForTest()) {
      // This should never take longer then a second.
      ASSERT_GE(1000, (base::TimeTicks::Now() - now).InMilliseconds());
      base::RunLoop().RunUntilIdle();
    }
  }

  // Return the window with the given index.
  aura::Window* window(size_t index) {
    DCHECK(index < windows_.size());
    return windows_[index];
  }

  // Delete the window at the given index, and set the referefence to NULL.
  void delete_window_at(size_t index) {
    delete windows_[index];
    windows_[index] = nullptr;
  }

  ash::MultiUserWindowManager* multi_user_window_manager() {
    return MultiUserWindowManagerHelper::GetWindowManager();
  }

  FakeChromeUserManager* user_manager() { return fake_user_manager_; }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  // Ensures that a user with the given |account_id| exists.
  const user_manager::User* EnsureTestUser(const AccountId& account_id) {
    const user_manager::User* user = fake_user_manager_->FindUser(account_id);
    if (user)
      return user;

    user = fake_user_manager_->AddUser(account_id);
    ash_test_helper()->test_session_controller_client()->AddUserSession(
        user->GetDisplayEmail());
    return user;
  }

  const user_manager::User* AddTestUser(const AccountId& account_id) {
    const user_manager::User* user = EnsureTestUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    TestingProfile* profile =
        profile_manager()->CreateTestingProfile(account_id.GetUserEmail());
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    return user;
  }

  // Returns a list of all open windows in the following form:
  // "<H(idden)/S(hown)/D(eleted)>[<Owner>[,<shownForUser>]], .."
  // Like: "S[b], .." would mean that window#0 is shown and belongs to user B.
  // or "S[b,a], .." would mean that window#0 is shown, belongs to B but is
  // shown by A, and "D,..." would mean that window#0 is deleted.
  std::string GetStatus() {
    return GetStatusImpl(/* follow_transients */ false);
  }

  // Same as GetStatus(), but uses the transient root to determine the owning
  // account.
  std::string GetStatusUseTransientOwners() {
    return GetStatusImpl(/* follow_transients */ true);
  }

  // Returns a test-friendly string format of GetOwnersOfVisibleWindows().
  std::string GetOwnersOfVisibleWindowsAsString();

  // Make a window system modal.
  void MakeWindowSystemModal(aura::Window* window) {
    aura::Window* system_modal_container =
        window->GetRootWindow()->GetChildById(
            kShellWindowId_SystemModalContainer);
    system_modal_container->AddChild(window);
  }

  void ShowWindowForUserNoUserTransition(aura::Window* window,
                                         const AccountId& account_id) {
    ash::MultiUserWindowManagerImpl::Get()->ShowWindowForUserIntern(window,
                                                                    account_id);
  }

  // The FakeChromeUserManager does not automatically call the window
  // manager. This function gets the current user from it and also sets it to
  // the multi user window manager.
  AccountId GetAndValidateCurrentUserFromSessionStateObserver() {
    GetSessionControllerClient()->FlushForTest();
    return Shell::Get()
        ->session_controller()
        ->GetUserSessions()[0]
        ->user_info.account_id;
  }

  // Initiate a user transition.
  void StartUserTransitionAnimation(const AccountId& account_id) {
    EnsureTestUser(account_id);
    ash_test_helper()->test_session_controller_client()->SwitchActiveUser(
        account_id);
  }

  // Call next animation step.
  void AdvanceUserTransitionAnimation() {
    ash::MultiUserWindowManagerImpl::Get()
        ->animation_->AdvanceUserTransitionAnimation();
  }

  // Return the user id of the wallpaper which is currently set.
  const std::string& GetWallpaperUserIdForTest() {
    return ash::MultiUserWindowManagerImpl::Get()
        ->animation_->wallpaper_user_id_for_test();
  }

  // Returns true if the given window covers the screen.
  bool CoversScreen(aura::Window* window) {
    return ash::UserSwitchAnimator::CoversScreen(window);
  }

 private:
  std::string GetStatusImpl(bool follow_transients);

  ScopedStubInstallAttributes test_install_attributes_;

  // These get created for each session.
  // TODO: convert to vector<std::unique_ptr<aura::Window>>.
  aura::Window::Windows windows_;

  std::unique_ptr<ash::CrosSettingsHolder> cros_settings_holder_;

  // Owned by |user_manager_enabler_|.
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> fake_user_manager_ =
      nullptr;

  std::unique_ptr<TestingProfileManager> profile_manager_;

  user_manager::ScopedUserManager user_manager_enabler_;

  // The maximized window manager (if enabled).
  std::unique_ptr<TabletModeWindowManager> tablet_mode_window_manager_;
};

void MultiProfileSupportTest::SetUp() {
  ash::DeviceSettingsService::Initialize();
  cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
      ash::DeviceSettingsService::Get(),
      TestingBrowserProcess::GetGlobal()->local_state());
  ChromeAshTestBase::SetUp(std::make_unique<TestShellDelegateChromeOS>());
  ash_test_helper()
      ->test_session_controller_client()
      ->set_use_lower_case_user_id(false);
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_.get()->SetUp());
  EnsureTestUser(AccountId::FromUserEmail("a"));
  EnsureTestUser(AccountId::FromUserEmail("b"));
  EnsureTestUser(AccountId::FromUserEmail("c"));
}

void MultiProfileSupportTest::SetUpForThisManyWindows(int windows) {
  ASSERT_TRUE(windows_.empty());
  for (int i = 0; i < windows; i++) {
    windows_.push_back(CreateTestWindowInShellWithId(i));
    windows_[i]->Show();
  }
  ::MultiUserWindowManagerHelper::CreateInstanceForTest(
      AccountId::FromUserEmail("a"));
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
}

std::vector<std::unique_ptr<views::Widget>>
MultiProfileSupportTest::SetUpOneWindowEachDeskForUser(AccountId account_id) {
  if (!windows_.empty())
    return std::vector<std::unique_ptr<views::Widget>>();
  std::vector<std::unique_ptr<views::Widget>> widgets;
  std::vector<int> container_ids = desks_util::GetDesksContainersIds();
  TestShellDelegate* test_shell_delegate =
      static_cast<TestShellDelegate*>(Shell::Get()->shell_delegate());
  // Set restore in progress to avoid activating desk activation during
  // `window->Show()` in `CreateTestWidget()`.
  test_shell_delegate->SetSessionRestoreInProgress(true);
  auto* desks_controller = ash::DesksController::Get();
  const int kActiveDeskIndex = 0;
  for (int i = 0; i < desks_controller->GetNumberOfDesks(); i++) {
    widgets.push_back(
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         nullptr, container_ids[i], gfx::Rect(700, 0, 50, 50)));
    aura::Window* win = widgets[i]->GetNativeWindow();
    windows_.push_back(win);
    // `TargetVisibility` is the local visibility of the window
    // regardless of the invisibility of its inactive parent desk.
    EXPECT_TRUE(win->TargetVisibility());
    // `IsVisible` is the window global visibility on the current workarea.
    // Thus, any window in non-active desk is considered invisible.
    EXPECT_EQ(i == kActiveDeskIndex, win->IsVisible());
    EXPECT_TRUE(ash::AutotestDesksApi().IsWindowInDesk(win,
                                                       /*desk_index=*/i));
  }
  EXPECT_EQ(kActiveDeskIndex, desks_controller->GetActiveDeskIndex());
  test_shell_delegate->SetSessionRestoreInProgress(false);
  return widgets;
}

void MultiProfileSupportTest::TearDown() {
  // Since the AuraTestBase is needed to create our assets, we have to
  // also delete them before we tear it down.
  while (!windows_.empty()) {
    delete *(windows_.begin());
    windows_.erase(windows_.begin());
  }

  ::MultiUserWindowManagerHelper::DeleteInstance();
  ChromeAshTestBase::TearDown();
  profile_manager_.reset();
  cros_settings_holder_.reset();
  ash::DeviceSettingsService::Shutdown();
}

std::string MultiProfileSupportTest::GetStatusImpl(bool follow_transients) {
  std::string s;
  for (size_t i = 0; i < windows_.size(); i++) {
    if (i)
      s += ", ";
    if (!window(i)) {
      s += "D";
      continue;
    }
    s += window(i)->IsVisible() ? "S[" : "H[";
    aura::Window* window_to_use_for_owner =
        follow_transients ? ::wm::GetTransientRoot(window(i)) : window(i);
    const AccountId& owner =
        multi_user_window_manager()->GetWindowOwner(window_to_use_for_owner);
    s += owner.GetUserEmail();
    const AccountId& presenter =
        multi_user_window_manager()->GetUserPresentingWindow(
            window_to_use_for_owner);
    if (!owner.empty() && owner != presenter) {
      s += ",";
      s += presenter.GetUserEmail();
    }
    s += "]";
  }
  return s;
}

std::string MultiProfileSupportTest::GetOwnersOfVisibleWindowsAsString() {
  std::set<AccountId> owners =
      multi_user_window_manager()->GetOwnersOfVisibleWindows();

  std::vector<std::string_view> owner_list;
  for (auto& owner : owners)
    owner_list.push_back(owner.GetUserEmail());
  return base::JoinString(owner_list, " ");
}

// Testing basic assumptions like default state and existence of manager.
TEST_F(MultiProfileSupportTest, BasicTests) {
  SetUpForThisManyWindows(3);
  // Check the basic assumptions: All windows are visible and there is no owner.
  EXPECT_EQ("S[], S[], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager());
  EXPECT_EQ(multi_user_window_manager(),
            ::MultiUserWindowManagerHelper::GetWindowManager());
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  // The owner of an unowned window should be empty and it should be shown on
  // all windows.
  EXPECT_FALSE(
      multi_user_window_manager()->GetWindowOwner(window(0)).is_valid());
  EXPECT_FALSE(multi_user_window_manager()
                   ->GetUserPresentingWindow(window(0))
                   .is_valid());
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(0), account_id_A));
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(0), account_id_B));

  // Set the owner of one window should remember it as such. It should only be
  // drawn on the owners desktop - not on any other.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  EXPECT_EQ(account_id_A,
            multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ(account_id_A,
            multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(0), account_id_A));
  EXPECT_FALSE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(0), account_id_B));

  // Overriding it with another state should show it on the other user's
  // desktop.
  ShowWindowForUserNoUserTransition(window(0), account_id_B);
  EXPECT_EQ(account_id_A,
            multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ(account_id_B,
            multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_FALSE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(0), account_id_A));
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(0), account_id_B));
}

// Testing simple owner changes.
TEST_F(MultiProfileSupportTest, OwnerTests) {
  SetUpForThisManyWindows(5);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Set some windows to the active owner.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  EXPECT_EQ("S[a], S[], S[], S[], S[]", GetStatus());
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_A);
  EXPECT_EQ("S[a], S[], S[a], S[], S[]", GetStatus());

  // Set some windows to an inactive owner. Note that the windows should hide.
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  EXPECT_EQ("S[a], H[b], S[a], S[], S[]", GetStatus());
  multi_user_window_manager()->SetWindowOwner(window(3), account_id_B);
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());

  // Assume that the user has now changed to C - which should show / hide
  // accordingly.
  StartUserTransitionAnimation(account_id_C);
  EXPECT_EQ("H[a], H[b], H[a], H[b], S[]", GetStatus());

  // If someone tries to show an inactive window it should only work if it can
  // be shown / hidden.
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());
  window(3)->Show();
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());
  window(2)->Hide();
  EXPECT_EQ("S[a], H[b], H[a], H[b], S[]", GetStatus());
  window(2)->Show();
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());
}

TEST_F(MultiProfileSupportTest, CloseWindowTests) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_B);
  EXPECT_EQ("H[b]", GetStatus());
  ShowWindowForUserNoUserTransition(window(0), account_id_A);
  EXPECT_EQ("S[b,a]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("b", GetOwnersOfVisibleWindowsAsString());

  aura::Window* to_be_deleted = window(0);

  EXPECT_EQ(account_id_A, multi_user_window_manager()->GetUserPresentingWindow(
                              to_be_deleted));
  EXPECT_EQ(account_id_B,
            multi_user_window_manager()->GetWindowOwner(to_be_deleted));

  // Close the window.
  delete_window_at(0);

  EXPECT_EQ("D", GetStatus());
  EXPECT_EQ("", GetOwnersOfVisibleWindowsAsString());
  // There should be no owner anymore for that window and the shared windows
  // should be gone as well.
  EXPECT_FALSE(multi_user_window_manager()
                   ->GetUserPresentingWindow(to_be_deleted)
                   .is_valid());
  EXPECT_FALSE(
      multi_user_window_manager()->GetWindowOwner(to_be_deleted).is_valid());
}

TEST_F(MultiProfileSupportTest, SharedWindowTests) {
  SetUpForThisManyWindows(5);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(3), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(4), account_id_C);
  EXPECT_EQ("S[a], S[a], H[b], H[b], H[c]", GetStatus());
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());

  // For all following tests we override window 2 to be shown by user B.
  ShowWindowForUserNoUserTransition(window(1), account_id_B);

  // Change window 3 between two users and see that it changes
  // accordingly (or not).
  ShowWindowForUserNoUserTransition(window(2), account_id_A);
  EXPECT_EQ("S[a], H[a,b], S[b,a], H[b], H[c]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("a b", GetOwnersOfVisibleWindowsAsString());
  ShowWindowForUserNoUserTransition(window(2), account_id_C);
  EXPECT_EQ("S[a], H[a,b], H[b,c], H[b], H[c]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());

  // Switch the users and see that the results are correct.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[a,b], H[b,c], S[b], H[c]", GetStatus());
  EXPECT_EQ("a b", GetOwnersOfVisibleWindowsAsString());
  StartUserTransitionAnimation(account_id_C);
  EXPECT_EQ("H[a], H[a,b], S[b,c], H[b], S[c]", GetStatus());
  EXPECT_EQ("b c", GetOwnersOfVisibleWindowsAsString());

  // Showing on the desktop of the already owning user should have no impact.
  ShowWindowForUserNoUserTransition(window(4), account_id_C);
  EXPECT_EQ("H[a], H[a,b], S[b,c], H[b], S[c]", GetStatus());
  EXPECT_EQ("b c", GetOwnersOfVisibleWindowsAsString());

  // Changing however a shown window back to the original owner should hide it.
  ShowWindowForUserNoUserTransition(window(2), account_id_B);
  EXPECT_EQ("H[a], H[a,b], H[b], H[b], S[c]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("c", GetOwnersOfVisibleWindowsAsString());

  // And the change should be "permanent" - switching somewhere else and coming
  // back.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[a,b], S[b], S[b], H[c]", GetStatus());
  EXPECT_EQ("a b", GetOwnersOfVisibleWindowsAsString());
  StartUserTransitionAnimation(account_id_C);
  EXPECT_EQ("H[a], H[a,b], H[b], H[b], S[c]", GetStatus());
  EXPECT_EQ("c", GetOwnersOfVisibleWindowsAsString());

  // After switching window 2 back to its original desktop, all desktops should
  // be "clean" again.
  ShowWindowForUserNoUserTransition(window(1), account_id_A);
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
}

// Make sure that adding a window to another desktop does not cause harm.
TEST_F(MultiProfileSupportTest, DoubleSharedWindowTests) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_B);

  // Add two references to the same window.
  ShowWindowForUserNoUserTransition(window(0), account_id_A);
  ShowWindowForUserNoUserTransition(window(0), account_id_A);
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());

  // Close the window.
  delete_window_at(0);

  EXPECT_EQ("D", GetStatus());
  // There should be no shares anymore open.
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
}

// Tests that the user's desktop visibility changes get respected. These tests
// are required to make sure that our usage of the same feature for showing and
// hiding does not interfere with the "normal operation".
TEST_F(MultiProfileSupportTest, PreserveWindowVisibilityTests) {
  SetUpForThisManyWindows(5);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Set some owners and make sure we got what we asked for.
  // Note that we try to cover all combinations in one go.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(3), account_id_B);
  ShowWindowForUserNoUserTransition(window(2), account_id_A);
  ShowWindowForUserNoUserTransition(window(3), account_id_A);
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());

  // Hiding a window should be respected - no matter if it is owned by that user
  // owned by someone else but shown on that desktop - or not owned.
  window(0)->Hide();
  window(2)->Hide();
  window(4)->Hide();
  EXPECT_EQ("H[a], S[a], H[b,a], S[b,a], H[]", GetStatus());

  // Flipping to another user and back should preserve all show / hide states.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], H[]", GetStatus());

  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("H[a], S[a], H[b,a], S[b,a], H[]", GetStatus());

  // After making them visible and switching fore and back everything should be
  // visible.
  window(0)->Show();
  window(2)->Show();
  window(4)->Show();
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());

  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], S[]", GetStatus());

  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());

  // Now test that making windows visible through "normal operation" while the
  // user's desktop is hidden leads to the correct result.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], S[]", GetStatus());
  window(0)->Show();
  window(2)->Show();
  window(4)->Show();
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], S[]", GetStatus());
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());
}

// Tests that windows in active and inactive desks show up correctly after
// switching profile (crbug.com/1182069). This test checks the followings:
// 1. window local visibility (appearance in desk miniviews) regardless
// of its ancestors' visibility like hidden parent desk container
// (see `Window::TargetVisibility()`).
// 2. window global visibility (appearance in the user screen) which takes
// its ancestor views' visibility into account (see `Window::IsVisible()`).
TEST_F(MultiProfileSupportTest, WindowVisibilityInMultipleDesksTests) {
  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  ::MultiUserWindowManagerHelper::CreateInstanceForTest(account_id_A);
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
  AddTestUser(account_id_A);
  AddTestUser(account_id_B);

  // In the user A, setup two desks with one window each.
  SwitchActiveUser(account_id_A);
  ash::AutotestDesksApi().CreateNewDesk();
  std::vector<std::unique_ptr<views::Widget>> widgets =
      SetUpOneWindowEachDeskForUser(account_id_A);
  ASSERT_FALSE(widgets.empty());
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_A);

  // Tests that both windows are locally visible, but only the first window
  // in the first active desk is globally visible.
  // GetStatus checks the global visibility `window::IsVisible()`.
  EXPECT_EQ("S[a], H[a]", GetStatus());
  // Local visibilties are true because both windows show up in desks miniview.
  EXPECT_TRUE(window(0)->TargetVisibility());
  EXPECT_TRUE(window(1)->TargetVisibility());

  // Tests that switching to userB globally and locally hides both userA's
  // windows.
  SwitchActiveUser(account_id_B);
  EXPECT_EQ("H[a], H[a]", GetStatus());
  EXPECT_FALSE(window(0)->TargetVisibility());
  EXPECT_FALSE(window(1)->TargetVisibility());

  // Tests that switching to userA globally shows both userA's windows, but does
  // not change windows' local visibility.
  SwitchActiveUser(account_id_A);
  EXPECT_EQ("S[a], H[a]", GetStatus());
  EXPECT_TRUE(window(0)->TargetVisibility());
  EXPECT_TRUE(window(1)->TargetVisibility());

  // Tests that activating the second desk globally shows userA's second window
  // but does not change windows' local visibility.
  auto* desk_2 = ash::DesksController::Get()->desks()[1].get();
  ash::ActivateDesk(desk_2);
  EXPECT_EQ("H[a], S[a]", GetStatus());
  EXPECT_TRUE(window(0)->TargetVisibility());
  EXPECT_TRUE(window(1)->TargetVisibility());

  delete_window_at(0);
  delete_window_at(1);
}

// Check that minimizing a window which is owned by another user will move it
// back and gets restored upon switching back to the original user.
TEST_F(MultiProfileSupportTest, MinimizeChangesOwnershipBack) {
  SetUpForThisManyWindows(4);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_B);
  ShowWindowForUserNoUserTransition(window(1), account_id_A);
  EXPECT_EQ("S[a], S[b,a], H[b], S[]", GetStatus());
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(1), account_id_A));
  WindowState::Get(window(1))->Minimize();
  // At this time the window is still on the desktop of that user, but the user
  // does not have a way to get to it.
  EXPECT_EQ("S[a], H[b,a], H[b], S[]", GetStatus());
  EXPECT_TRUE(
      MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
          window(1), account_id_A));
  EXPECT_TRUE(WindowState::Get(window(1))->IsMinimized());
  // Change to user B and make sure that minimizing does not change anything.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[b], S[b], S[]", GetStatus());
  EXPECT_FALSE(WindowState::Get(window(1))->IsMinimized());
}

// Check that we cannot transfer the ownership of a minimized window.
TEST_F(MultiProfileSupportTest, MinimizeSuppressesViewTransfer) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  WindowState::Get(window(0))->Minimize();
  EXPECT_EQ("H[a]", GetStatus());

  // Try to transfer the window to user B - which should get ignored.
  ShowWindowForUserNoUserTransition(window(0), account_id_B);
  EXPECT_EQ("H[a]", GetStatus());
}

// Testing that the activation state changes to the active window.
TEST_F(MultiProfileSupportTest, ActiveWindowTests) {
  SetUpForThisManyWindows(4);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Set some windows to the active owner.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(3), account_id_B);
  EXPECT_EQ("S[a], S[a], H[b], H[b]", GetStatus());

  // Set the active window for user A to be #1
  ::wm::ActivateWindow(window(1));

  // Change to user B and make sure that one of its windows is active.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], H[a], S[b], S[b]", GetStatus());
  EXPECT_TRUE(::wm::IsActiveWindow(window(3)) ||
              ::wm::IsActiveWindow(window(2)));
  // Set the active window for user B now to be #2
  ::wm::ActivateWindow(window(2));

  StartUserTransitionAnimation(account_id_A);
  EXPECT_TRUE(::wm::IsActiveWindow(window(1)));

  StartUserTransitionAnimation(account_id_B);
  EXPECT_TRUE(::wm::IsActiveWindow(window(2)));

  StartUserTransitionAnimation(account_id_C);
  ::wm::ActivationClient* activation_client =
      ::wm::GetActivationClient(window(0)->GetRootWindow());
  EXPECT_EQ(nullptr, activation_client->GetActiveWindow());

  // Now test that a minimized window stays minimized upon switch and back.
  StartUserTransitionAnimation(account_id_A);
  WindowState::Get(window(0))->Minimize();

  StartUserTransitionAnimation(account_id_B);
  StartUserTransitionAnimation(account_id_A);
  EXPECT_TRUE(WindowState::Get(window(0))->IsMinimized());
  EXPECT_TRUE(::wm::IsActiveWindow(window(1)));
}

// Test that Transient windows are handled properly.
TEST_F(MultiProfileSupportTest, TransientWindows) {
  SetUpForThisManyWindows(10);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  // We create a hierarchy like this:
  //    0 (A)  4 (B)   7 (-)   - The top level owned/not owned windows
  //    |      |       |
  //    1      5 - 6   8       - Transient child of the owned windows.
  //    |              |
  //    2              9       - A transtient child of a transient child.
  //    |
  //    3                      - ..
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(4), account_id_B);
  ::wm::AddTransientChild(window(0), window(1));
  // We first attach 2->3 and then 1->2 to see that the ownership gets
  // properly propagated through the sub tree upon assigning.
  ::wm::AddTransientChild(window(2), window(3));
  ::wm::AddTransientChild(window(1), window(2));
  ::wm::AddTransientChild(window(4), window(5));
  ::wm::AddTransientChild(window(4), window(6));
  ::wm::AddTransientChild(window(7), window(8));
  ::wm::AddTransientChild(window(7), window(9));

  // By now the hierarchy should have updated itself to show all windows of A
  // and hide all windows of B. Unowned windows should remain in what ever state
  // they are in.
  EXPECT_EQ("S[a], S[], S[], S[], H[b], H[], H[], S[], S[], S[]", GetStatus());

  // Trying to show a hidden transient window shouldn't change anything for now.
  window(5)->Show();
  window(6)->Show();
  EXPECT_EQ("S[a], S[], S[], S[], H[b], H[], H[], S[], S[], S[]", GetStatus());

  // Hiding on the other hand a shown window should work and hide also its
  // children. Note that hide will have an immediate impact on itself and all
  // transient children. It furthermore should remember this state when the
  // transient children are removed from its owner later on.
  window(2)->Hide();
  window(9)->Hide();
  EXPECT_EQ("S[a], S[], H[], H[], H[b], H[], H[], S[], S[], H[]", GetStatus());

  // Switching users and switch back should return to the previous state.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], H[], H[], H[], S[b], S[], S[], S[], S[], H[]", GetStatus());
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("S[a], S[], H[], H[], H[b], H[], H[], S[], S[], H[]", GetStatus());

  // Removing a window from its transient parent should return to the previously
  // set visibility state.
  // Note: Window2 was explicitly hidden above and that state should remain.
  // Note furthermore that Window3 should also be hidden since it was hidden
  // implicitly by hiding Window2.
  // set hidden above).
  //    0 (A)  4 (B)   7 (-)   2(-)   3 (-)    6(-)
  //    |      |       |
  //    1      5       8
  //                   |
  //                   9
  ::wm::RemoveTransientChild(window(2), window(3));
  ::wm::RemoveTransientChild(window(4), window(6));
  EXPECT_EQ("S[a], S[], H[], H[], H[b], H[], S[], S[], S[], H[]", GetStatus());
  // Before we leave we need to reverse all transient window ownerships.
  ::wm::RemoveTransientChild(window(0), window(1));
  ::wm::RemoveTransientChild(window(1), window(2));
  ::wm::RemoveTransientChild(window(4), window(5));
  ::wm::RemoveTransientChild(window(7), window(8));
  ::wm::RemoveTransientChild(window(7), window(9));
}

// Verifies duplicate observers are not added for transient dialog windows.
// https://crbug.com/937333
TEST_F(MultiProfileSupportTest, SetWindowOwnerOnTransientDialog) {
  SetUpForThisManyWindows(2);
  aura::Window* parent = window(0);
  aura::Window* transient = window(1);
  const AccountId account_id(AccountId::FromUserEmail("a"));
  multi_user_window_manager()->SetWindowOwner(parent, account_id);

  // Simulate chrome::ShowWebDialog() showing a transient dialog, which calls
  // SetWindowOwner() on the transient.
  ::wm::AddTransientChild(parent, transient);
  multi_user_window_manager()->SetWindowOwner(transient, account_id);

  // Both windows are shown and owned by user A.
  EXPECT_EQ("S[a], S[a]", GetStatusUseTransientOwners());

  // Cleanup.
  ::wm::RemoveTransientChild(parent, transient);
}

// Test that the initial visibility state gets remembered.
TEST_F(MultiProfileSupportTest, PreserveInitialVisibility) {
  SetUpForThisManyWindows(4);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  // Set our initial show state before we assign an owner.
  window(0)->Show();
  window(1)->Hide();
  window(2)->Show();
  window(3)->Hide();
  EXPECT_EQ("S[], H[], S[], H[]", GetStatus());

  // First test: The show state gets preserved upon user switch.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(3), account_id_B);
  EXPECT_EQ("S[a], H[a], H[b], H[b]", GetStatus());
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], H[a], S[b], H[b]", GetStatus());
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("S[a], H[a], H[b], H[b]", GetStatus());

  // Second test: Transferring the window to another desktop preserves the
  // show state.
  ShowWindowForUserNoUserTransition(window(0), account_id_B);
  ShowWindowForUserNoUserTransition(window(1), account_id_B);
  ShowWindowForUserNoUserTransition(window(2), account_id_A);
  ShowWindowForUserNoUserTransition(window(3), account_id_A);
  EXPECT_EQ("H[a,b], H[a,b], S[b,a], H[b,a]", GetStatus());
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("S[a,b], H[a,b], H[b,a], H[b,a]", GetStatus());
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("H[a,b], H[a,b], S[b,a], H[b,a]", GetStatus());
}

// Test that in case of an activated tablet mode, windows from all users get
// maximized on entering tablet mode.
TEST_F(MultiProfileSupportTest, TabletModeInteraction) {
  SetUpForThisManyWindows(2);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);

  EXPECT_FALSE(WindowState::Get(window(0))->IsMaximized());
  EXPECT_FALSE(WindowState::Get(window(1))->IsMaximized());

  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_TRUE(WindowState::Get(window(0))->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window(1))->IsMaximized());

  // Tests that on exiting tablet mode, the window states return to not
  // maximized.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(WindowState::Get(window(0))->IsMaximized());
  EXPECT_FALSE(WindowState::Get(window(1))->IsMaximized());
}

// Test that a system modal dialog will switch to the desktop of the owning
// user.
TEST_F(MultiProfileSupportTest, SwitchUsersUponModalityChange) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_a(AccountId::FromUserEmail("a"));
  const AccountId account_id_b(AccountId::FromUserEmail("b"));

  StartUserTransitionAnimation(account_id_a);

  // Making the window system modal should not change anything.
  MakeWindowSystemModal(window(0));
  EXPECT_EQ(account_id_a, GetAndValidateCurrentUserFromSessionStateObserver());

  // Making the window owned by user B should switch users.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_b);
  EXPECT_EQ(account_id_b, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that a system modal dialog will not switch desktop if active user has
// shows window.
TEST_F(MultiProfileSupportTest, DontSwitchUsersUponModalityChange) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_a(AccountId::FromUserEmail("a"));
  const AccountId account_id_b(AccountId::FromUserEmail("b"));

  StartUserTransitionAnimation(account_id_a);

  // Making the window system modal should not change anything.
  MakeWindowSystemModal(window(0));
  EXPECT_EQ(account_id_a, GetAndValidateCurrentUserFromSessionStateObserver());

  // Making the window owned by user a should not switch users.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_a);
  EXPECT_EQ(account_id_a, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that a system modal dialog will not switch if shown on correct desktop
// but owned by another user.
TEST_F(MultiProfileSupportTest,
       DontSwitchUsersUponModalityChangeWhenShownButNotOwned) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_a(AccountId::FromUserEmail("a"));
  const AccountId account_id_b(AccountId::FromUserEmail("b"));

  StartUserTransitionAnimation(account_id_a);

  window(0)->Hide();
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_b);
  ShowWindowForUserNoUserTransition(window(0), account_id_a);
  MakeWindowSystemModal(window(0));
  // Showing the window should trigger no user switch.
  window(0)->Show();
  EXPECT_EQ(account_id_a, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that a system modal dialog will switch if shown on incorrect desktop but
// even if owned by current user.
TEST_F(MultiProfileSupportTest,
       SwitchUsersUponModalityChangeWhenShownButNotOwned) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_a(AccountId::FromUserEmail("a"));
  const AccountId account_id_b(AccountId::FromUserEmail("b"));

  StartUserTransitionAnimation(account_id_a);

  window(0)->Hide();
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_a);
  ShowWindowForUserNoUserTransition(window(0), account_id_b);
  MakeWindowSystemModal(window(0));
  // Showing the window should trigger a user switch.
  window(0)->Show();
  EXPECT_EQ(account_id_b, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that using the full user switch animations are working as expected.
TEST_F(MultiProfileSupportTest, FullUserSwitchAnimationTests) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_C);
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());

  // Switch the user fore and back and see that the results are correct.
  SwitchUserAndWaitForAnimation(account_id_B);

  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ("b", GetOwnersOfVisibleWindowsAsString());

  SwitchUserAndWaitForAnimation(account_id_A);

  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());

  // Switch the user quickly to another user and before the animation is done
  // switch back and see that this works.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  // Check that after switching to C, C is fully visible.
  SwitchUserAndWaitForAnimation(account_id_C);
  EXPECT_EQ("H[a], H[b], S[c]", GetStatus());
  EXPECT_EQ("c", GetOwnersOfVisibleWindowsAsString());
}

// Make sure that we do not crash upon shutdown when an animation is pending and
// a shutdown happens.
TEST_F(MultiProfileSupportTest, SystemShutdownWithActiveAnimation) {
  SetUpForThisManyWindows(2);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  StartUserTransitionAnimation(account_id_B);
  // We don't do anything more here - the animations are pending and with the
  // shutdown of the framework the animations should get cancelled. If not a
  // crash would happen.
}

// Test that using the full user switch, the animations are transitioning as
// we expect them to in all animation steps.
TEST_F(MultiProfileSupportTest, AnimationSteps) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_C);
  EXPECT_FALSE(CoversScreen(window(0)));
  EXPECT_FALSE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is becoming invisible, the
  // new one is becoming visible, and the background starts transitionining.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ(kArrowBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  AdvanceUserTransitionAnimation();
  EXPECT_EQ(kArrowBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // After the finalize the animation of the wallpaper should be finished.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
}

// Test that the screen coverage is properly determined.
TEST_F(MultiProfileSupportTest, AnimationStepsScreenCoverage) {
  SetUpForThisManyWindows(3);
  // Maximizing, fully covering the screen by bounds or fullscreen mode should
  // make CoversScreen return true.
  WindowState::Get(window(0))->Maximize();
  window(1)->SetBounds(gfx::Rect(0, 0, 3000, 3000));

  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_FALSE(CoversScreen(window(2)));

  WMEvent event(WM_EVENT_FULLSCREEN);
  WindowState::Get(window(2))->OnWMEvent(&event);
  EXPECT_TRUE(CoversScreen(window(2)));
}

// Test that switching from a desktop which has a maximized window to a desktop
// which has no maximized window will produce the proper animation.
TEST_F(MultiProfileSupportTest, AnimationStepsMaximizeToNormal) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  WindowState::Get(window(0))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_C);
  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_FALSE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the new background is immediately set.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The final step will also not have any visible impact.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());
}

// Test that switching from a desktop which has a normal window to a desktop
// which has a maximized window will produce the proper animation.
TEST_F(MultiProfileSupportTest, AnimationStepsNormalToMaximized) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  WindowState::Get(window(1))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_C);
  EXPECT_FALSE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is becoming invisible, the
  // new one visible and the background remains as is.
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ("", GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ("", GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The final step however will switch the background.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());
}

// Test that switching from a desktop which has a maximized window to a desktop
// which has a maximized window will produce the proper animation.
TEST_F(MultiProfileSupportTest, AnimationStepsMaximizedToMaximized) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  const AccountId account_id_C(AccountId::FromUserEmail("c"));

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  WindowState::Get(window(0))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_B);
  WindowState::Get(window(1))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_C);
  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the all windows are hidden (except that of
  // the new user).
  StartUserTransitionAnimation(account_id_B);
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The final step however will hide the old window.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ(kBAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // Switching back will do the exact same thing.
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ(kAAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(0.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ(kAAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(0.0f, window(1)->layer()->GetTargetOpacity());

  // The final step is also not changing anything to the status.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ(kAAccountIdString, GetWallpaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(0.0f, window(1)->layer()->GetTargetOpacity());
}

// Test that showing a window for another user also switches the desktop.
TEST_F(MultiProfileSupportTest, ShowForUserSwitchesDesktop) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_a(AccountId::FromUserEmail("a"));
  const AccountId account_id_b(AccountId::FromUserEmail("b"));
  const AccountId account_id_c(AccountId::FromUserEmail("c"));

  StartUserTransitionAnimation(account_id_a);

  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_a);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_b);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_c);
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());

  // SetWindowOwner should not have changed the active user.
  EXPECT_EQ(account_id_a, GetAndValidateCurrentUserFromSessionStateObserver());

  // Check that teleporting the window of the currently active user will
  // teleport to the new desktop.
  multi_user_window_manager()->ShowWindowForUser(window(0), account_id_b);
  EXPECT_EQ(account_id_b, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], H[c]", GetStatus());

  // Check that teleporting a window from a currently inactive user will not
  // trigger a switch.
  multi_user_window_manager()->ShowWindowForUser(window(2), account_id_a);
  EXPECT_EQ(account_id_b, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], H[c,a]", GetStatus());
  multi_user_window_manager()->ShowWindowForUser(window(2), account_id_b);
  EXPECT_EQ(account_id_b, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], S[c,b]", GetStatus());

  // Check that teleporting back will also change the desktop.
  multi_user_window_manager()->ShowWindowForUser(window(2), account_id_c);
  EXPECT_EQ(account_id_c, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("H[a,b], H[b], S[c]", GetStatus());
}

class TestWindowObserver : public aura::WindowObserver {
 public:
  TestWindowObserver() : resize_calls_(0) {}

  TestWindowObserver(const TestWindowObserver&) = delete;
  TestWindowObserver& operator=(const TestWindowObserver&) = delete;

  ~TestWindowObserver() override {}

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    resize_calls_++;
  }

  int resize_calls() { return resize_calls_; }

 private:
  int resize_calls_;
};

// Test that switching between different user won't change the activated windows
// and the property of transient windows.
TEST_F(MultiProfileSupportTest, TransientWindowActivationTest) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));

  // Create a window hierarchy like this:
  // 0 (A)          - The normal windows
  // |
  // 1              - Transient child of the normal windows.
  // |
  // 2              - A transient child of a transient child.

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);

  ::wm::AddTransientChild(window(0), window(1));
  window(1)->SetProperty(aura::client::kModalKey,
                         ui::mojom::ModalType::kWindow);

  ::wm::AddTransientChild(window(1), window(2));
  window(2)->SetProperty(aura::client::kModalKey,
                         ui::mojom::ModalType::kWindow);

  ::wm::ActivationClient* activation_client =
      ::wm::GetActivationClient(window(0)->GetRootWindow());

  // Activate window #0 will activate its deepest transient child window #2.
  activation_client->ActivateWindow(window(0));
  EXPECT_EQ(window(2), activation_client->GetActiveWindow());
  EXPECT_FALSE(::wm::CanActivateWindow(window(0)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(1)));

  // Change active user to User B.
  StartUserTransitionAnimation(account_id_B);

  // Change active user back to User A.
  StartUserTransitionAnimation(account_id_A);
  EXPECT_EQ(window(2), activation_client->GetActiveWindow());
  EXPECT_FALSE(::wm::CanActivateWindow(window(0)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(1)));

  // Test that switching user doesn't change the property of the windows.
  EXPECT_EQ(ui::mojom::ModalType::kNone,
            window(0)->GetProperty(aura::client::kModalKey));
  EXPECT_EQ(ui::mojom::ModalType::kWindow,
            window(1)->GetProperty(aura::client::kModalKey));
  EXPECT_EQ(ui::mojom::ModalType::kWindow,
            window(2)->GetProperty(aura::client::kModalKey));

  ::wm::RemoveTransientChild(window(0), window(1));
  ::wm::RemoveTransientChild(window(1), window(2));
}

// Test that minimized window on one desktop can't be activated on another
// desktop.
TEST_F(MultiProfileSupportTest, MinimizedWindowActivatableTests) {
  SetUpForThisManyWindows(4);

  const AccountId user1(AccountId::FromUserEmail("a@test.com"));
  const AccountId user2(AccountId::FromUserEmail("b@test.com"));
  AddTestUser(user1);
  AddTestUser(user2);

  multi_user_window_manager()->SetWindowOwner(window(0), user1);
  multi_user_window_manager()->SetWindowOwner(window(1), user1);
  multi_user_window_manager()->SetWindowOwner(window(2), user2);
  multi_user_window_manager()->SetWindowOwner(window(3), user2);

  // Minimizes window #0 and window #2.
  WindowState::Get(window(0))->Minimize();
  WindowState::Get(window(2))->Minimize();

  // Windows belonging to user2 (window #2 and #3) can't be activated by user1.
  SwitchActiveUser(user1);
  EXPECT_TRUE(::wm::CanActivateWindow(window(0)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(1)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(2)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(3)));

  // Windows belonging to user1 (window #0 and #1) can't be activated by user2.
  SwitchActiveUser(user2);
  EXPECT_FALSE(::wm::CanActivateWindow(window(0)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(1)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(2)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(3)));
}

// Test that teleported window can be activated by the presenting user.
TEST_F(MultiProfileSupportTest, TeleportedWindowActivatableTests) {
  // The synchronously SwitchActiveUser of session controller (and client)
  // breaks the test as it tests the transient state in middle of user
  // switching. Since the test itself does user switching, disable the one
  // in session controller by resetting the client.
  Shell::Get()->session_controller()->SetClient(nullptr);

  SetUpForThisManyWindows(2);

  const AccountId user1(AccountId::FromUserEmail("a@test.com"));
  const AccountId user2(AccountId::FromUserEmail("b@test.com"));
  AddTestUser(user1);
  AddTestUser(user2);

  multi_user_window_manager()->SetWindowOwner(window(0), user1);
  multi_user_window_manager()->SetWindowOwner(window(1), user2);

  SwitchActiveUser(user1);
  EXPECT_TRUE(::wm::CanActivateWindow(window(0)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(1)));

  // Teleports window #0 to user2 desktop. Then window #0 can't be activated by
  // user 1.
  multi_user_window_manager()->ShowWindowForUser(window(0), user2);
  EXPECT_FALSE(::wm::CanActivateWindow(window(0)));

  // Test that window #0 can be activated by user2.
  SwitchActiveUser(user2);
  EXPECT_TRUE(::wm::CanActivateWindow(window(0)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(1)));
}

// Test that teleported window has the kAvatarIconKey window property.
TEST_F(MultiProfileSupportTest, TeleportedWindowAvatarProperty) {
  SetUpForThisManyWindows(1);

  const AccountId user1(AccountId::FromUserEmail("a@test.com"));
  const AccountId user2(AccountId::FromUserEmail("b@test.com"));
  AddTestUser(user1);
  AddTestUser(user2);

  multi_user_window_manager()->SetWindowOwner(window(0), user1);

  SwitchActiveUser(user1);

  aura::Window* property_window = window(0);

  // Window #0 has no kAvatarIconKey property before teleporting.
  EXPECT_FALSE(property_window->GetProperty(aura::client::kAvatarIconKey));

  // Teleport window #0 to user2 and kAvatarIconKey property is present.
  multi_user_window_manager()->ShowWindowForUser(window(0), user2);
  EXPECT_TRUE(property_window->GetProperty(aura::client::kAvatarIconKey));

  // Teleport window #0 back to its owner (user1) and kAvatarIconKey property is
  // gone.
  multi_user_window_manager()->ShowWindowForUser(window(0), user1);
  EXPECT_FALSE(property_window->GetProperty(aura::client::kAvatarIconKey));
}

// Tests that the window order is preserved when switching between users. Also
// tests that the window's activation is restored correctly if one user's MRU
// window list is empty.
TEST_F(MultiProfileSupportTest, WindowsOrderPreservedTests) {
  SetUpForThisManyWindows(3);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  AddTestUser(account_id_A);
  AddTestUser(account_id_B);
  SwitchActiveUser(account_id_A);

  // Set the windows owner.
  ::wm::ActivationClient* activation_client =
      ::wm::GetActivationClient(window(0)->GetRootWindow());
  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(1), account_id_A);
  multi_user_window_manager()->SetWindowOwner(window(2), account_id_A);
  EXPECT_EQ("S[a], S[a], S[a]", GetStatus());

  // Activate the windows one by one.
  activation_client->ActivateWindow(window(2));
  activation_client->ActivateWindow(window(1));
  activation_client->ActivateWindow(window(0));
  EXPECT_EQ(activation_client->GetActiveWindow(), window(0));

  aura::Window::Windows mru_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_EQ(mru_list[0], window(0));
  EXPECT_EQ(mru_list[1], window(1));
  EXPECT_EQ(mru_list[2], window(2));

  SwitchActiveUser(account_id_B);
  EXPECT_EQ("H[a], H[a], H[a]", GetStatus());
  EXPECT_EQ(activation_client->GetActiveWindow(), nullptr);

  SwitchActiveUser(account_id_A);
  EXPECT_EQ("S[a], S[a], S[a]", GetStatus());
  EXPECT_EQ(activation_client->GetActiveWindow(), window(0));

  mru_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_EQ(mru_list[0], window(0));
  EXPECT_EQ(mru_list[1], window(1));
  EXPECT_EQ(mru_list[2], window(2));
}

// Tests that chrome::FindBrowserWithActiveWindow works properly in
// multi-user scenario, that is it should return the browser with active window
// associated with it (crbug.com/675265).
TEST_F(MultiProfileSupportTest, FindBrowserWithActiveWindow) {
  SetUpForThisManyWindows(1);

  const AccountId account_id_A(AccountId::FromUserEmail("a"));
  const AccountId account_id_B(AccountId::FromUserEmail("b"));
  AddTestUser(account_id_A);
  AddTestUser(account_id_B);
  SwitchActiveUser(account_id_A);

  multi_user_window_manager()->SetWindowOwner(window(0), account_id_A);
  Profile* profile = multi_user_util::GetProfileFromAccountId(account_id_A);
  Browser::CreateParams params(profile, true);
  std::unique_ptr<Browser> browser(CreateTestBrowser(
      CreateTestWindowInShellWithId(0), gfx::Rect(16, 32, 640, 320), &params));
  browser->window()->Activate();
  // Manually set last active browser in BrowserList for testing.
  BrowserList::GetInstance()->SetLastActive(browser.get());
  EXPECT_EQ(browser.get(), BrowserList::GetInstance()->GetLastActive());
  EXPECT_TRUE(browser->window()->IsActive());
  EXPECT_EQ(browser.get(), chrome::FindBrowserWithActiveWindow());

  // Switch to another user's desktop with no active window.
  SwitchActiveUser(account_id_B);
  EXPECT_EQ(browser.get(), BrowserList::GetInstance()->GetLastActive());
  EXPECT_FALSE(browser->window()->IsActive());
  EXPECT_EQ(nullptr, chrome::FindBrowserWithActiveWindow());
}

// Tests that a window's bounds get restored to their pre tablet mode bounds,
// even on a secondary user and with display rotations.
TEST_F(MultiProfileSupportTest, WindowBoundsAfterTabletMode) {
  UpdateDisplay("400x200");
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Add two windows, one to each user and set their initial bounds.
  SetUpForThisManyWindows(2);
  const AccountId user1(AccountId::FromUserEmail("a"));
  const AccountId user2(AccountId::FromUserEmail("b"));
  AddTestUser(user1);
  AddTestUser(user2);
  SwitchActiveUser(user1);
  multi_user_window_manager()->SetWindowOwner(window(0), user1);
  multi_user_window_manager()->SetWindowOwner(window(1), user2);
  const gfx::Rect bounds(20, 20, 360, 100);
  window(0)->SetBounds(bounds);
  window(1)->SetBounds(bounds);

  // Enter tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  // Tests that bounds of both windows are maximized.
  const gfx::Rect maximized_bounds(0, 0, 400,
                                   200 - ShelfConfig::Get()->shelf_size());
  EXPECT_EQ(maximized_bounds, window(0)->bounds());
  EXPECT_EQ(maximized_bounds, window(1)->bounds());

  // Rotate to portrait and back to trigger some display changes.
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  ash::TabletModeControllerTestApi().LeaveTabletMode();

  // Tests that both windows have the same bounds as when they entered tablet
  // mode.
  SwitchActiveUser(user2);
  EXPECT_EQ(bounds, window(0)->bounds());
  EXPECT_EQ(bounds, window(1)->bounds());
}

TEST_F(MultiProfileSupportTest, AccountIdChangesAfterSwitch) {
  SetUpForThisManyWindows(1);

  const AccountId account1(AccountId::FromUserEmail("a"));
  const AccountId account2(AccountId::FromUserEmail("b"));
  AddTestUser(account1);
  AddTestUser(account2);
  SwitchActiveUser(account1);
  EXPECT_EQ(account1, multi_user_window_manager()->CurrentAccountId());

  SwitchActiveUser(account2);
  EXPECT_EQ(account2, multi_user_window_manager()->CurrentAccountId());
}

}  // namespace ash

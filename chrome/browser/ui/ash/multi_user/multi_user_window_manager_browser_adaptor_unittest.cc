// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_browser_adaptor.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/multi_user/user_switch_animator.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
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
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/ash/new_window/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/user_login_permission_tracker.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
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

constexpr auto kAccountIdA =
    AccountId::Literal::FromUserEmailGaiaId("a", GaiaId::Literal("gaia_a"));
constexpr auto kAccountIdB =
    AccountId::Literal::FromUserEmailGaiaId("b", GaiaId::Literal("gaia_b"));
constexpr auto kAccountIdC =
    AccountId::Literal::FromUserEmailGaiaId("c", GaiaId::Literal("gaia_c"));

const char kAAccountIdString[] =
    R"({"account_type":"google","email":"a","gaia_id":"gaia_a"})";
const char kBAccountIdString[] =
    R"({"account_type":"google","email":"b","gaia_id":"gaia_b"})";
const char kArrowBAccountIdString[] =
    R"(->{"account_type":"google","email":"b","gaia_id":"gaia_b"})";

const content::BrowserContext* GetActiveContext() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user) {
    return nullptr;
  }
  return ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user);
}

class TestShellDelegateChromeOS : public ash::TestShellDelegate {
 public:
  TestShellDelegateChromeOS() = default;

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
  if (!bounds.IsEmpty()) {
    window->SetBounds(bounds);
  }
  std::unique_ptr<Browser> browser =
      chrome::CreateBrowserWithAuraTestWindowForParams(base::WrapUnique(window),
                                                       params);
  return browser;
}

}  // namespace

namespace ash {

// A test class for preparing the MultiUserWindowManager. It creates
// various windows and instantiates the MultiUserWindowManager.
class MultiUserWindowManagerBrowserAdaptorTest : public ChromeAshTestBase {
 public:
  MultiUserWindowManagerBrowserAdaptorTest() { set_start_session(false); }

  MultiUserWindowManagerBrowserAdaptorTest(
      const MultiUserWindowManagerBrowserAdaptorTest&) = delete;
  MultiUserWindowManagerBrowserAdaptorTest& operator=(
      const MultiUserWindowManagerBrowserAdaptorTest&) = delete;

  // ChromeAshTestBase:
  void SetUp() override;
  void TearDown() override;
  void OnHelperWillBeDestroyed() override;

 protected:
  void SwitchActiveUser(const AccountId& account_id) {
    CHECK(user_manager_->FindUser(account_id));
    user_manager_->SwitchActiveUser(account_id);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  // Set up the test environment for this many windows.
  void SetUpForThisManyWindows(int windows);

  // If |windows_| is empty, set up one window each desk for a given user
  // without activating any desk and return a list of created widgets.
  // Otherwise, do nothing and return an empty vector.
  std::vector<std::unique_ptr<views::Widget>> SetUpOneWindowEachDeskForUser();

  // Switch the user and wait until the animation is finished.
  void SwitchUserAndWaitForAnimation(const AccountId& account_id) {
    CHECK(user_manager_->FindUser(account_id));
    SwitchActiveUser(account_id);

    base::TimeTicks now = base::TimeTicks::Now();
    while (ash::MultiUserWindowManager::Get()->IsAnimationRunningForTest()) {
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
    return ash::Shell::Get()->multi_user_window_manager();
  }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  void AddUser(const AccountId& account_id) {
    ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                    .AddRegularUser(account_id));
  }

  void LogInUser(const AccountId& account_id) {
    const auto* user = user_manager_->FindUser(account_id);
    CHECK(user);
    user_manager_->UserLoggedIn(
        user->GetAccountId(),
        user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));

    TestingProfile* profile = nullptr;
    {
      ash::ScopedAccountIdAnnotator annotator(
          profile_manager_->profile_manager(), user->GetAccountId());
      profile = profile_manager_->CreateTestingProfile(
          user->GetAccountId().GetUserEmail());
    }

    user_manager_->OnUserProfileCreated(user->GetAccountId(),
                                        profile->GetPrefs());
    GetSessionControllerClient()->SetUnownedUserPrefService(
        user->GetAccountId(), profile->GetPrefs());

    GetSessionControllerClient()->AddUserSession(
        {user->GetDisplayEmail(), user->GetType()}, user->GetAccountId());
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);

    // Workaround with testing utilities. Currently, for primary user login
    // case, OnActiveUserSessionChanged is not called via
    // TestSessionControllerClient, so call it manually here.
    // TODO(crbug.com/425160398): Make TestSessionControllerClient behavior
    // closer to the production one, or get rid of it to connect actual
    // UserManager and SessionManager in tests.
    if (user_manager_->GetPrimaryUser() == user) {
      ash::MultiUserWindowManager::Get()->OnActiveUserSessionChanged(
          user->GetAccountId());
    }

    if (user_manager_->GetActiveUser() != user) {
      user_manager_->SwitchActiveUser(user->GetAccountId());
      GetSessionControllerClient()->SwitchActiveUser(user->GetAccountId());
    }
  }

  void AddLoggedInUsers(base::span<const AccountId> ids) {
    // This must run only once, and when this is called,
    // there should be no logged in users yet.
    CHECK(user_manager::UserManager::Get()->GetLoggedInUsers().empty());
    CHECK(!ids.empty());

    for (const AccountId& account_id : ids) {
      AddUser(account_id);
    }

    // Primary user log-in.
    LogInUser(ids[0]);

    for (const AccountId& account_id : ids.subspan(1u)) {
      LogInUser(account_id);
      multi_user_window_manager_browser_adaptor_->AddUser(account_id);
    }
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
    ash::MultiUserWindowManager::Get()->ShowWindowForUserIntern(window,
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

  // Call next animation step.
  void AdvanceUserTransitionAnimation() {
    ash::MultiUserWindowManager::Get()
        ->animation_->AdvanceUserTransitionAnimation();
  }

  // Return the user id of the wallpaper which is currently set.
  const std::string& GetWallpaperUserIdForTest() {
    return ash::MultiUserWindowManager::Get()
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
  std::unique_ptr<ash::UserLoginPermissionTracker>
      user_login_permission_tracker_;

  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<MultiUserWindowManagerBrowserAdaptor>
      multi_user_window_manager_browser_adaptor_;

  // The maximized window manager (if enabled).
  std::unique_ptr<TabletModeWindowManager> tablet_mode_window_manager_;

  std::optional<ash::BrowserControllerImpl> browser_controller_;
};

void MultiUserWindowManagerBrowserAdaptorTest::SetUp() {
  ash::DeviceSettingsService::Initialize();
  cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
      ash::DeviceSettingsService::Get(),
      TestingBrowserProcess::GetGlobal()->local_state());
  user_login_permission_tracker_ =
      std::make_unique<UserLoginPermissionTracker>(ash::CrosSettings::Get());

  user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
      std::make_unique<user_manager::FakeUserManagerDelegate>(),
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState(),
      ash::CrosSettings::Get()));
  set_shell_delegate(std::make_unique<TestShellDelegateChromeOS>());

  ChromeAshTestBase::SetUp();
  GetSessionControllerClient()->set_pref_service_must_exist(true);

  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_DISABLED);

  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());

  multi_user_window_manager_browser_adaptor_ =
      std::make_unique<MultiUserWindowManagerBrowserAdaptor>(
          ash::Shell::Get()->multi_user_window_manager());

  browser_controller_.emplace();
}

void MultiUserWindowManagerBrowserAdaptorTest::SetUpForThisManyWindows(
    int windows) {
  CHECK(user_manager_->GetActiveUser());

  ASSERT_TRUE(windows_.empty());
  for (int i = 0; i < windows; i++) {
    windows_.push_back(CreateTestWindowInShell({.window_id = i}));
    windows_[i]->Show();
  }
}

std::vector<std::unique_ptr<views::Widget>>
MultiUserWindowManagerBrowserAdaptorTest::SetUpOneWindowEachDeskForUser() {
  if (!windows_.empty()) {
    return std::vector<std::unique_ptr<views::Widget>>();
  }
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

void MultiUserWindowManagerBrowserAdaptorTest::TearDown() {
  browser_controller_.reset();

  // Since the AuraTestBase is needed to create our assets, we have to
  // also delete them before we tear it down.
  while (!windows_.empty()) {
    delete *(windows_.begin());
    windows_.erase(windows_.begin());
  }

  multi_user_window_manager_browser_adaptor_.reset();
  for (Profile* profile :
       profile_manager_->profile_manager()->GetLoadedProfiles()) {
    const AccountId* account_id = ash::AnnotatedAccountId::Get(profile);
    if (account_id) {
      user_manager_->OnUserProfileWillBeDestroyed(*account_id);
    }
  }
  ChromeAshTestBase::TearDown();
  // ProfileManager instance is destroyed in OnHelperWillBeDestroyed()
  // invoked inside ChromeAshTestBase::TearDown().
  EXPECT_FALSE(profile_manager_.get());
  user_manager_.Reset();
  user_login_permission_tracker_.reset();
  cros_settings_holder_.reset();
  ash::DeviceSettingsService::Shutdown();
}

void MultiUserWindowManagerBrowserAdaptorTest::OnHelperWillBeDestroyed() {
  ChromeAshTestBase::OnHelperWillBeDestroyed();
  profile_manager_.reset();
}

std::string MultiUserWindowManagerBrowserAdaptorTest::GetStatusImpl(
    bool follow_transients) {
  std::string s;
  for (size_t i = 0; i < windows_.size(); i++) {
    if (i) {
      s += ", ";
    }
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

std::string
MultiUserWindowManagerBrowserAdaptorTest::GetOwnersOfVisibleWindowsAsString() {
  std::set<AccountId> owners =
      multi_user_window_manager()->GetOwnersOfVisibleWindows();

  std::vector<std::string_view> owner_list;
  for (auto& owner : owners) {
    owner_list.push_back(owner.GetUserEmail());
  }
  return base::JoinString(owner_list, " ");
}

// Testing basic assumptions like default state and existence of manager.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, BasicTests) {
  AddLoggedInUsers(
      {AccountId::FromUserEmailGaiaId("user@test", GaiaId("123456789"))});
  SetUpForThisManyWindows(3);
  // Check the basic assumptions: All windows are visible and there is no owner.
  EXPECT_EQ("S[], S[], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager());
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());

  // The owner of an unowned window should be empty and it should be shown on
  // all windows.
  EXPECT_FALSE(
      multi_user_window_manager()->GetWindowOwner(window(0)).is_valid());
  EXPECT_FALSE(multi_user_window_manager()
                   ->GetUserPresentingWindow(window(0))
                   .is_valid());
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(0), kAccountIdA));
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(0), kAccountIdB));

  // Set the owner of one window should remember it as such. It should only be
  // drawn on the owners desktop - not on any other.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  EXPECT_EQ(kAccountIdA,
            multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ(kAccountIdA,
            multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(0), kAccountIdA));
  EXPECT_FALSE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(0), kAccountIdB));

  // Overriding it with another state should show it on the other user's
  // desktop.
  ShowWindowForUserNoUserTransition(window(0), kAccountIdB);
  EXPECT_EQ(kAccountIdA,
            multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ(kAccountIdB,
            multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_FALSE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(0), kAccountIdA));
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(0), kAccountIdB));
}

// Testing simple owner changes.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, OwnerTests) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});
  SetUpForThisManyWindows(5);

  // Set some windows to the active owner.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  EXPECT_EQ("S[a], S[], S[], S[], S[]", GetStatus());
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdA);
  EXPECT_EQ("S[a], S[], S[a], S[], S[]", GetStatus());

  // Set some windows to an inactive owner. Note that the windows should hide.
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  EXPECT_EQ("S[a], H[b], S[a], S[], S[]", GetStatus());
  multi_user_window_manager()->SetWindowOwner(window(3), kAccountIdB);
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());

  // Assume that the user has now changed to C - which should show / hide
  // accordingly.
  SwitchActiveUser(kAccountIdC);
  EXPECT_EQ("H[a], H[b], H[a], H[b], S[]", GetStatus());

  // If someone tries to show an inactive window it should only work if it can
  // be shown / hidden.
  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());
  window(3)->Show();
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());
  window(2)->Hide();
  EXPECT_EQ("S[a], H[b], H[a], H[b], S[]", GetStatus());
  window(2)->Show();
  EXPECT_EQ("S[a], H[b], S[a], H[b], S[]", GetStatus());
}

TEST_F(MultiUserWindowManagerBrowserAdaptorTest, CloseWindowTests) {
  AddLoggedInUsers({kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(1);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdB);
  EXPECT_EQ("H[b]", GetStatus());
  ShowWindowForUserNoUserTransition(window(0), kAccountIdA);
  EXPECT_EQ("S[b,a]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("b", GetOwnersOfVisibleWindowsAsString());

  aura::Window* to_be_deleted = window(0);

  EXPECT_EQ(kAccountIdA, multi_user_window_manager()->GetUserPresentingWindow(
                             to_be_deleted));
  EXPECT_EQ(kAccountIdB,
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

TEST_F(MultiUserWindowManagerBrowserAdaptorTest, SharedWindowTests) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(5);

  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(3), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(4), kAccountIdC);
  EXPECT_EQ("S[a], S[a], H[b], H[b], H[c]", GetStatus());
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());

  // For all following tests we override window 2 to be shown by user B.
  ShowWindowForUserNoUserTransition(window(1), kAccountIdB);

  // Change window 3 between two users and see that it changes
  // accordingly (or not).
  ShowWindowForUserNoUserTransition(window(2), kAccountIdA);
  EXPECT_EQ("S[a], H[a,b], S[b,a], H[b], H[c]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("a b", GetOwnersOfVisibleWindowsAsString());
  ShowWindowForUserNoUserTransition(window(2), kAccountIdC);
  EXPECT_EQ("S[a], H[a,b], H[b,c], H[b], H[c]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());

  // Switch the users and see that the results are correct.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], S[a,b], H[b,c], S[b], H[c]", GetStatus());
  EXPECT_EQ("a b", GetOwnersOfVisibleWindowsAsString());
  SwitchActiveUser(kAccountIdC);
  EXPECT_EQ("H[a], H[a,b], S[b,c], H[b], S[c]", GetStatus());
  EXPECT_EQ("b c", GetOwnersOfVisibleWindowsAsString());

  // Showing on the desktop of the already owning user should have no impact.
  ShowWindowForUserNoUserTransition(window(4), kAccountIdC);
  EXPECT_EQ("H[a], H[a,b], S[b,c], H[b], S[c]", GetStatus());
  EXPECT_EQ("b c", GetOwnersOfVisibleWindowsAsString());

  // Changing however a shown window back to the original owner should hide it.
  ShowWindowForUserNoUserTransition(window(2), kAccountIdB);
  EXPECT_EQ("H[a], H[a,b], H[b], H[b], S[c]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("c", GetOwnersOfVisibleWindowsAsString());

  // And the change should be "permanent" - switching somewhere else and coming
  // back.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], S[a,b], S[b], S[b], H[c]", GetStatus());
  EXPECT_EQ("a b", GetOwnersOfVisibleWindowsAsString());
  SwitchActiveUser(kAccountIdC);
  EXPECT_EQ("H[a], H[a,b], H[b], H[b], S[c]", GetStatus());
  EXPECT_EQ("c", GetOwnersOfVisibleWindowsAsString());

  // After switching window 2 back to its original desktop, all desktops should
  // be "clean" again.
  ShowWindowForUserNoUserTransition(window(1), kAccountIdA);
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
}

// Make sure that adding a window to another desktop does not cause harm.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, DoubleSharedWindowTests) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdB);

  // Add two references to the same window.
  ShowWindowForUserNoUserTransition(window(0), kAccountIdA);
  ShowWindowForUserNoUserTransition(window(0), kAccountIdA);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       PreserveWindowVisibilityTests) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});
  SetUpForThisManyWindows(5);

  // Set some owners and make sure we got what we asked for.
  // Note that we try to cover all combinations in one go.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(3), kAccountIdB);
  ShowWindowForUserNoUserTransition(window(2), kAccountIdA);
  ShowWindowForUserNoUserTransition(window(3), kAccountIdA);
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());

  // Hiding a window should be respected - no matter if it is owned by that user
  // owned by someone else but shown on that desktop - or not owned.
  window(0)->Hide();
  window(2)->Hide();
  window(4)->Hide();
  EXPECT_EQ("H[a], S[a], H[b,a], S[b,a], H[]", GetStatus());

  // Flipping to another user and back should preserve all show / hide states.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], H[]", GetStatus());

  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ("H[a], S[a], H[b,a], S[b,a], H[]", GetStatus());

  // After making them visible and switching fore and back everything should be
  // visible.
  window(0)->Show();
  window(2)->Show();
  window(4)->Show();
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());

  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], S[]", GetStatus());

  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());

  // Now test that making windows visible through "normal operation" while the
  // user's desktop is hidden leads to the correct result.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], S[]", GetStatus());
  window(0)->Show();
  window(2)->Show();
  window(4)->Show();
  EXPECT_EQ("H[a], H[a], H[b,a], H[b,a], S[]", GetStatus());
  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ("S[a], S[a], S[b,a], S[b,a], S[]", GetStatus());
}

// Tests that windows in active and inactive desks show up correctly after
// switching profile (crbug.com/1182069). This test checks the followings:
// 1. window local visibility (appearance in desk miniviews) regardless
// of its ancestors' visibility like hidden parent desk container
// (see `Window::TargetVisibility()`).
// 2. window global visibility (appearance in the user screen) which takes
// its ancestor views' visibility into account (see `Window::IsVisible()`).
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       WindowVisibilityInMultipleDesksTests) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  // In the user A, setup two desks with one window each.
  SwitchActiveUser(kAccountIdA);
  ash::AutotestDesksApi().CreateNewDesk();
  std::vector<std::unique_ptr<views::Widget>> widgets =
      SetUpOneWindowEachDeskForUser();
  ASSERT_FALSE(widgets.empty());
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);

  // Tests that both windows are locally visible, but only the first window
  // in the first active desk is globally visible.
  // GetStatus checks the global visibility `window::IsVisible()`.
  EXPECT_EQ("S[a], H[a]", GetStatus());
  // Local visibilties are true because both windows show up in desks miniview.
  EXPECT_TRUE(window(0)->TargetVisibility());
  EXPECT_TRUE(window(1)->TargetVisibility());

  // Tests that switching to userB globally and locally hides both userA's
  // windows.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a]", GetStatus());
  EXPECT_FALSE(window(0)->TargetVisibility());
  EXPECT_FALSE(window(1)->TargetVisibility());

  // Tests that switching to userA globally shows both userA's windows, but does
  // not change windows' local visibility.
  SwitchActiveUser(kAccountIdA);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, MinimizeChangesOwnershipBack) {
  AddLoggedInUsers({kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(4);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdB);
  ShowWindowForUserNoUserTransition(window(1), kAccountIdA);
  EXPECT_EQ("S[a], S[b,a], H[b], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(1), kAccountIdA));
  WindowState::Get(window(1))->Minimize();
  // At this time the window is still on the desktop of that user, but the user
  // does not have a way to get to it.
  EXPECT_EQ("S[a], H[b,a], H[b], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(
      window(1), kAccountIdA));
  EXPECT_TRUE(WindowState::Get(window(1))->IsMinimized());
  // Change to user B and make sure that minimizing does not change anything.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], S[b], S[b], S[]", GetStatus());
  EXPECT_FALSE(WindowState::Get(window(1))->IsMinimized());
}

// Check that we cannot transfer the ownership of a minimized window.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       MinimizeSuppressesViewTransfer) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  WindowState::Get(window(0))->Minimize();
  EXPECT_EQ("H[a]", GetStatus());

  // Try to transfer the window to user B - which should get ignored.
  ShowWindowForUserNoUserTransition(window(0), kAccountIdB);
  EXPECT_EQ("H[a]", GetStatus());
}

// Testing that the activation state changes to the active window.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, ActiveWindowTests) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(4);

  // Set some windows to the active owner.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(3), kAccountIdB);
  EXPECT_EQ("S[a], S[a], H[b], H[b]", GetStatus());

  // Set the active window for user A to be #1
  ::wm::ActivateWindow(window(1));

  // Change to user B and make sure that one of its windows is active.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a], S[b], S[b]", GetStatus());
  EXPECT_TRUE(::wm::IsActiveWindow(window(3)) ||
              ::wm::IsActiveWindow(window(2)));
  // Set the active window for user B now to be #2
  ::wm::ActivateWindow(window(2));

  SwitchActiveUser(kAccountIdA);
  EXPECT_TRUE(::wm::IsActiveWindow(window(1)));

  SwitchActiveUser(kAccountIdB);
  EXPECT_TRUE(::wm::IsActiveWindow(window(2)));

  SwitchActiveUser(kAccountIdC);
  ::wm::ActivationClient* activation_client =
      ::wm::GetActivationClient(window(0)->GetRootWindow());
  EXPECT_EQ(nullptr, activation_client->GetActiveWindow());

  // Now test that a minimized window stays minimized upon switch and back.
  SwitchActiveUser(kAccountIdA);
  WindowState::Get(window(0))->Minimize();

  SwitchActiveUser(kAccountIdB);
  SwitchActiveUser(kAccountIdA);
  EXPECT_TRUE(WindowState::Get(window(0))->IsMinimized());
  EXPECT_TRUE(::wm::IsActiveWindow(window(1)));
}

// Test that Transient windows are handled properly.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, TransientWindows) {
  AddLoggedInUsers({kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(10);

  // We create a hierarchy like this:
  //    0 (A)  4 (B)   7 (-)   - The top level owned/not owned windows
  //    |      |       |
  //    1      5 - 6   8       - Transient child of the owned windows.
  //    |              |
  //    2              9       - A transtient child of a transient child.
  //    |
  //    3                      - ..
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(4), kAccountIdB);
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
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[], H[], H[], S[b], S[], S[], S[], S[], H[]", GetStatus());
  SwitchActiveUser(kAccountIdA);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       SetWindowOwnerOnTransientDialog) {
  AddLoggedInUsers({kAccountIdA});

  SetUpForThisManyWindows(2);
  aura::Window* parent = window(0);
  aura::Window* transient = window(1);

  multi_user_window_manager()->SetWindowOwner(parent, kAccountIdA);

  // Simulate chrome::ShowWebDialog() showing a transient dialog, which calls
  // SetWindowOwner() on the transient.
  ::wm::AddTransientChild(parent, transient);
  multi_user_window_manager()->SetWindowOwner(transient, kAccountIdA);

  // Both windows are shown and owned by user A.
  EXPECT_EQ("S[a], S[a]", GetStatusUseTransientOwners());

  // Cleanup.
  ::wm::RemoveTransientChild(parent, transient);
}

// Test that the initial visibility state gets remembered.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, PreserveInitialVisibility) {
  AddLoggedInUsers({kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(4);

  // Set our initial show state before we assign an owner.
  window(0)->Show();
  window(1)->Hide();
  window(2)->Show();
  window(3)->Hide();
  EXPECT_EQ("S[], H[], S[], H[]", GetStatus());

  // First test: The show state gets preserved upon user switch.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(3), kAccountIdB);
  EXPECT_EQ("S[a], H[a], H[b], H[b]", GetStatus());
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a], S[b], H[b]", GetStatus());
  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ("S[a], H[a], H[b], H[b]", GetStatus());

  // Second test: Transferring the window to another desktop preserves the
  // show state.
  ShowWindowForUserNoUserTransition(window(0), kAccountIdB);
  ShowWindowForUserNoUserTransition(window(1), kAccountIdB);
  ShowWindowForUserNoUserTransition(window(2), kAccountIdA);
  ShowWindowForUserNoUserTransition(window(3), kAccountIdA);
  EXPECT_EQ("H[a,b], H[a,b], S[b,a], H[b,a]", GetStatus());
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("S[a,b], H[a,b], H[b,a], H[b,a]", GetStatus());
  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ("H[a,b], H[a,b], S[b,a], H[b,a]", GetStatus());
}

// Test that in case of an activated tablet mode, windows from all users get
// maximized on entering tablet mode.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, TabletModeInteraction) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(2);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);

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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       SwitchUsersUponModalityChange) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  SwitchActiveUser(kAccountIdA);

  // Making the window system modal should not change anything.
  MakeWindowSystemModal(window(0));
  EXPECT_EQ(kAccountIdA, GetAndValidateCurrentUserFromSessionStateObserver());

  // Making the window owned by user B should switch users.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdB);
  EXPECT_EQ(kAccountIdB, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that a system modal dialog will not switch desktop if active user has
// shows window.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       DontSwitchUsersUponModalityChange) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  SwitchActiveUser(kAccountIdA);

  // Making the window system modal should not change anything.
  MakeWindowSystemModal(window(0));
  EXPECT_EQ(kAccountIdA, GetAndValidateCurrentUserFromSessionStateObserver());

  // Making the window owned by user a should not switch users.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  EXPECT_EQ(kAccountIdA, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that a system modal dialog will not switch if shown on correct desktop
// but owned by another user.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       DontSwitchUsersUponModalityChangeWhenShownButNotOwned) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  SwitchActiveUser(kAccountIdA);

  window(0)->Hide();
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdB);
  ShowWindowForUserNoUserTransition(window(0), kAccountIdA);
  MakeWindowSystemModal(window(0));
  // Showing the window should trigger no user switch.
  window(0)->Show();
  EXPECT_EQ(kAccountIdA, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that a system modal dialog will switch if shown on incorrect desktop but
// even if owned by current user.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       SwitchUsersUponModalityChangeWhenShownButNotOwned) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  SwitchActiveUser(kAccountIdA);

  window(0)->Hide();
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  ShowWindowForUserNoUserTransition(window(0), kAccountIdB);
  MakeWindowSystemModal(window(0));
  // Showing the window should trigger a user switch.
  window(0)->Show();
  EXPECT_EQ(kAccountIdB, GetAndValidateCurrentUserFromSessionStateObserver());
}

// Test that using the full user switch animations are working as expected.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, FullUserSwitchAnimationTests) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(3);

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdC);
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());

  // Switch the user fore and back and see that the results are correct.
  SwitchUserAndWaitForAnimation(kAccountIdB);

  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  EXPECT_EQ("b", GetOwnersOfVisibleWindowsAsString());

  SwitchUserAndWaitForAnimation(kAccountIdA);

  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());

  // Switch the user quickly to another user and before the animation is done
  // switch back and see that this works.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], S[b], H[c]", GetStatus());
  // Check that after switching to C, C is fully visible.
  SwitchUserAndWaitForAnimation(kAccountIdC);
  EXPECT_EQ("H[a], H[b], S[c]", GetStatus());
  EXPECT_EQ("c", GetOwnersOfVisibleWindowsAsString());
}

// Make sure that we do not crash upon shutdown when an animation is pending and
// a shutdown happens.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       SystemShutdownWithActiveAnimation) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB, kAccountIdC});

  SetUpForThisManyWindows(2);

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  SwitchActiveUser(kAccountIdB);
  // We don't do anything more here - the animations are pending and with the
  // shutdown of the framework the animations should get cancelled. If not a
  // crash would happen.
}

// Test that using the full user switch, the animations are transitioning as
// we expect them to in all animation steps.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, AnimationSteps) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(3);

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdC);
  EXPECT_FALSE(CoversScreen(window(0)));
  EXPECT_FALSE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is becoming invisible, the
  // new one is becoming visible, and the background starts transitionining.
  SwitchActiveUser(kAccountIdB);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, AnimationStepsScreenCoverage) {
  AddLoggedInUsers({kAccountIdA});

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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       AnimationStepsMaximizeToNormal) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(3);

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  WindowState::Get(window(0))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdC);
  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_FALSE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the new background is immediately set.
  SwitchActiveUser(kAccountIdB);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       AnimationStepsNormalToMaximized) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(3);

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  WindowState::Get(window(1))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdC);
  EXPECT_FALSE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is becoming invisible, the
  // new one visible and the background remains as is.
  SwitchActiveUser(kAccountIdB);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       AnimationStepsMaximizedToMaximized) {
  AddLoggedInUsers({kAccountIdC, kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(3);

  // Turn the use of delays and animation on.
  ash::MultiUserWindowManager::Get()->SetAnimationSpeedForTest(
      ash::MultiUserWindowManager::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  WindowState::Get(window(0))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  WindowState::Get(window(1))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdC);
  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());
  EXPECT_EQ("a", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the all windows are hidden (except that of
  // the new user).
  SwitchActiveUser(kAccountIdB);
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
  SwitchActiveUser(kAccountIdA);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, ShowForUserSwitchesDesktop) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB, kAccountIdC});

  SetUpForThisManyWindows(3);

  SwitchActiveUser(kAccountIdA);

  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdC);
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());

  // SetWindowOwner should not have changed the active user.
  EXPECT_EQ(kAccountIdA, GetAndValidateCurrentUserFromSessionStateObserver());

  // Check that teleporting the window of the currently active user will
  // teleport to the new desktop.
  multi_user_window_manager()->ShowWindowForUser(window(0), kAccountIdB);
  EXPECT_EQ(kAccountIdB, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], H[c]", GetStatus());

  // Check that teleporting a window from a currently inactive user will not
  // trigger a switch.
  multi_user_window_manager()->ShowWindowForUser(window(2), kAccountIdA);
  EXPECT_EQ(kAccountIdB, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], H[c,a]", GetStatus());
  multi_user_window_manager()->ShowWindowForUser(window(2), kAccountIdB);
  EXPECT_EQ(kAccountIdB, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], S[c,b]", GetStatus());

  // Check that teleporting back will also change the desktop.
  multi_user_window_manager()->ShowWindowForUser(window(2), kAccountIdC);
  EXPECT_EQ(kAccountIdC, GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("H[a,b], H[b], S[c]", GetStatus());
}

class TestWindowObserver : public aura::WindowObserver {
 public:
  TestWindowObserver() : resize_calls_(0) {}

  TestWindowObserver(const TestWindowObserver&) = delete;
  TestWindowObserver& operator=(const TestWindowObserver&) = delete;

  ~TestWindowObserver() override = default;

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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       TransientWindowActivationTest) {
  AddLoggedInUsers({kAccountIdB, kAccountIdA});

  SetUpForThisManyWindows(3);

  // Create a window hierarchy like this:
  // 0 (A)          - The normal windows
  // |
  // 1              - Transient child of the normal windows.
  // |
  // 2              - A transient child of a transient child.

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);

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
  SwitchActiveUser(kAccountIdB);

  // Change active user back to User A.
  SwitchActiveUser(kAccountIdA);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       MinimizedWindowActivatableTests) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(4);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdB);
  multi_user_window_manager()->SetWindowOwner(window(3), kAccountIdB);

  // Minimizes window #0 and window #2.
  WindowState::Get(window(0))->Minimize();
  WindowState::Get(window(2))->Minimize();

  // Windows belonging to user2 (window #2 and #3) can't be activated by user1.
  SwitchActiveUser(kAccountIdA);
  EXPECT_TRUE(::wm::CanActivateWindow(window(0)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(1)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(2)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(3)));

  // Windows belonging to user1 (window #0 and #1) can't be activated by user2.
  SwitchActiveUser(kAccountIdB);
  EXPECT_FALSE(::wm::CanActivateWindow(window(0)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(1)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(2)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(3)));
}

// Test that teleported window can be activated by the presenting user.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       TeleportedWindowActivatableTests) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(2);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);

  SwitchActiveUser(kAccountIdA);
  EXPECT_TRUE(::wm::CanActivateWindow(window(0)));
  EXPECT_FALSE(::wm::CanActivateWindow(window(1)));

  // Teleports window #0 to user2 desktop, without switching to user 2.  Then
  // window #0 can't be activated by user 1. This scenario doesn't happen on
  // production but is kept instead of removed.
  ShowWindowForUserNoUserTransition(window(0), kAccountIdB);
  EXPECT_FALSE(::wm::CanActivateWindow(window(0)));

  // Test that window #0 can be activated by user2.
  SwitchActiveUser(kAccountIdB);
  EXPECT_TRUE(::wm::CanActivateWindow(window(0)));
  EXPECT_TRUE(::wm::CanActivateWindow(window(1)));
}

// Test that teleported window has the kAvatarIconKey window property.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest,
       TeleportedWindowAvatarProperty) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);

  SwitchActiveUser(kAccountIdA);

  aura::Window* property_window = window(0);

  // Window #0 has no kAvatarIconKey property before teleporting.
  EXPECT_FALSE(property_window->GetProperty(aura::client::kAvatarIconKey));

  // Teleport window #0 to user2 and kAvatarIconKey property is present.
  multi_user_window_manager()->ShowWindowForUser(window(0), kAccountIdB);
  EXPECT_TRUE(property_window->GetProperty(aura::client::kAvatarIconKey));

  // Teleport window #0 back to its owner (user1) and kAvatarIconKey property is
  // gone.
  multi_user_window_manager()->ShowWindowForUser(window(0), kAccountIdA);
  EXPECT_FALSE(property_window->GetProperty(aura::client::kAvatarIconKey));
}

// Tests that the window order is preserved when switching between users. Also
// tests that the window's activation is restored correctly if one user's MRU
// window list is empty.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, WindowsOrderPreservedTests) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(3);

  SwitchActiveUser(kAccountIdA);

  // Set the windows owner.
  ::wm::ActivationClient* activation_client =
      ::wm::GetActivationClient(window(0)->GetRootWindow());
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(2), kAccountIdA);
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

  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ("H[a], H[a], H[a]", GetStatus());
  EXPECT_EQ(activation_client->GetActiveWindow(), nullptr);

  SwitchActiveUser(kAccountIdA);
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
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, FindBrowserWithActiveWindow) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  SwitchActiveUser(kAccountIdA);

  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          kAccountIdA));
  Browser::CreateParams params(profile, true);
  std::unique_ptr<Browser> browser(CreateTestBrowser(
      CreateTestWindowInShell({.window_id = 0}), {16, 32, 640, 320}, &params));
  browser->window()->Activate();
  // Manually set last active browser in BrowserList for testing.
  BrowserList::GetInstance()->SetLastActive(browser.get());
  EXPECT_EQ(browser.get(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());
  EXPECT_TRUE(browser->window()->IsActive());
  EXPECT_EQ(browser.get(), chrome::FindBrowserWithActiveWindow());

  // Switch to another user's desktop with no active window.
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ(browser.get(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());
  EXPECT_FALSE(browser->window()->IsActive());
  EXPECT_EQ(nullptr, chrome::FindBrowserWithActiveWindow());
}

// Tests that a window's bounds get restored to their pre tablet mode bounds,
// even on a secondary user and with display rotations.
TEST_F(MultiUserWindowManagerBrowserAdaptorTest, WindowBoundsAfterTabletMode) {
  UpdateDisplay("400x200");
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(),
      display::Screen::Get()->GetPrimaryDisplay().id());

  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  // Add two windows, one to each user and set their initial bounds.
  SetUpForThisManyWindows(2);
  SwitchActiveUser(kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(0), kAccountIdA);
  multi_user_window_manager()->SetWindowOwner(window(1), kAccountIdB);
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
  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ(bounds, window(0)->bounds());
  EXPECT_EQ(bounds, window(1)->bounds());
}

TEST_F(MultiUserWindowManagerBrowserAdaptorTest, AccountIdChangesAfterSwitch) {
  AddLoggedInUsers({kAccountIdA, kAccountIdB});

  SetUpForThisManyWindows(1);

  SwitchActiveUser(kAccountIdA);
  EXPECT_EQ(kAccountIdA, multi_user_window_manager()->CurrentAccountId());

  SwitchActiveUser(kAccountIdB);
  EXPECT_EQ(kAccountIdB, multi_user_window_manager()->CurrentAccountId());
}

}  // namespace ash

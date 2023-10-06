// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/password_manager/web_app_profile_switcher.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/views/controls/styled_label.h"

namespace views {
class Button;
}

struct CoreAccountInfo;
class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileMenuView : public ProfileMenuViewBase {
 public:
  ProfileMenuView(views::Button* anchor_button, Browser* browser);
  ~ProfileMenuView() override;

  ProfileMenuView(const ProfileMenuView&) = delete;
  ProfileMenuView& operator=(const ProfileMenuView&) = delete;

  // ProfileMenuViewBase:
  void BuildMenu() override;
  gfx::ImageSkia GetSyncIcon() const override;

 private:
  friend class ProfileMenuViewExtensionsTest;
  friend class ProfileMenuViewSignoutTest;
  friend class ProfileMenuViewSyncErrorButtonTest;
  friend class ProfileMenuInteractiveUiTest;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  friend class ProfileMenuViewSigninErrorButtonTest;
#endif

  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  // Button/link actions.
  void OnManageGoogleAccountButtonClicked();
  void OnPasswordsButtonClicked();
  void OnCreditCardsButtonClicked();
  void OnAddressesButtonClicked();
  void OnGuestProfileButtonClicked();
  void OnExitProfileButtonClicked();
  void OnSyncSettingsButtonClicked();
  void OnSyncErrorButtonClicked(AvatarSyncErrorType error);
  void OnSigninButtonClicked(CoreAccountInfo account,
                             ActionableItem button_type);
  void OnCookiesClearedOnExitLinkClicked();
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnSignoutButtonClicked();
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void OnOtherProfileSelected(const base::FilePath& profile_path);
  void OnAddNewProfileButtonClicked();
  void OnManageProfilesButtonClicked();
  void OnEditProfileButtonClicked();
#endif

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  // Helper methods for building the menu.
  void BuildIdentity();
  void BuildGuestIdentity();
  void BuildAutofillButtons();
  void BuildSyncInfo();
  void BuildFeatureButtons();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void BuildAvailableProfiles();
  void BuildProfileManagementFeatureButtons();
#endif

  std::u16string menu_title_;
  std::u16string menu_subtitle_;

#if !BUILDFLAG(IS_CHROMEOS)
  // A profile switcher object needed if the user triggers opening other
  // profile in a web app.
  absl::optional<WebAppProfileSwitcher> app_profile_switcher_;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

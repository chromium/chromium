// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/web_app_profile_switcher.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/views/controls/styled_label.h"

namespace signin_metrics {
enum class AccessPoint;
}

namespace views {
class Button;
}

struct CoreAccountInfo;
class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
//
// If `explicit_signin_access_point` is provided, it will be used as the access
// point for the signin (or sync) flow. This is used to track the real source of
// the signin (or sync), e.g. history sync opt-in identity pill promo.
class ProfileMenuView : public ProfileMenuViewBase {
 public:
  ProfileMenuView(views::Button* anchor_button,
                  Browser* browser,
                  std::optional<signin_metrics::AccessPoint>
                      explicit_signin_access_point = std::nullopt);
  ~ProfileMenuView() override;

  ProfileMenuView(const ProfileMenuView&) = delete;
  ProfileMenuView& operator=(const ProfileMenuView&) = delete;

  // ProfileMenuViewBase:
  void BuildMenu() override;

 private:
  friend class ProfileMenuViewExtensionsTest;
  friend class ProfileMenuViewSigninPendingTest;
  friend class ProfileMenuViewSignoutTest;
  friend class ProfileMenuViewSyncErrorButtonTest;
  friend class ProfileMenuInteractiveUiTest;

  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  // Button/link actions.
  void OnProfileManagementButtonClicked();
  void OnManageGoogleAccountButtonClicked();
  void OnPasswordsButtonClicked();
  void OnCreditCardsButtonClicked();
  void OnAddressesButtonClicked();
  void OnGuestProfileButtonClicked();
  void OnExitProfileButtonClicked();
  void OnSyncSettingsButtonClicked();
  void OnSyncErrorButtonClicked(AvatarSyncErrorType error);
  void OnSigninButtonClicked(CoreAccountInfo account,
                             ActionableItem button_type,
                             signin_metrics::AccessPoint access_point);
  void OnCookiesClearedOnExitLinkClicked();
  void OnSignoutButtonClicked();
  void OnOtherProfileSelected(const base::FilePath& profile_path);
  void OnAddNewProfileButtonClicked();
  void OnManageProfilesButtonClicked();
  void OnEditProfileButtonClicked();
  void OnAutofillSettingsButtonClicked();

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  // Helper methods for building the menu.
  void SetMenuTitleForAccessibility();
  void BuildGuestIdentity();
  void BuildHistorySyncOptInButton();
  void BuildAutofillSettingsButton();
  void BuildCustomizeProfileButton();
  void MaybeBuildChromeAccountSettingsButton();
  void MaybeBuildManageGoogleAccountButton();
  void MaybeBuildCloseBrowsersButton();
  void MaybeBuildSignoutButton();
  void BuildFeatureButtons();
  IdentitySectionParams GetIdentitySectionParams(
      const ProfileAttributesEntry& entry);
  void BuildIdentityWithCallToAction();

  // Gets the profiles to be displayed in the "Other profiles" section. Does not
  // include the current profile.
  void GetProfilesForOtherProfilesSection(
      std::vector<ProfileAttributesEntry*>& available_profiles) const;
  void BuildOtherProfilesSection(
      const std::vector<ProfileAttributesEntry*>& available_profiles);

  void BuildProfileManagementFeatureButtons();

  std::u16string menu_title_;
  std::u16string menu_subtitle_;

  // A profile switcher object needed if the user triggers opening other
  // profile in a web app.
  std::optional<WebAppProfileSwitcher> app_profile_switcher_;

  std::optional<signin_metrics::AccessPoint> explicit_signin_access_point_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

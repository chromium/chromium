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
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "ui/views/controls/styled_label.h"

namespace signin_metrics {
enum class AccessPoint;
}

namespace ui {
class TrackedElement;
}  // namespace ui

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
//
// Dismissing the menu without clicking an actionable item will trigger a HaTS
// survey.
class ProfileMenuView : public ProfileMenuViewBase {
 public:
  // `browser` must not be nullptr.
  ProfileMenuView(ui::TrackedElement* anchor_element,
                  Browser* browser,
                  signin::ProfileMenuAvatarButtonPromoInfo promo_info,
                  bool from_avatar_promo);
  ~ProfileMenuView() override;

  ProfileMenuView(const ProfileMenuView&) = delete;
  ProfileMenuView& operator=(const ProfileMenuView&) = delete;

  // ProfileMenuViewBase:
  void BuildMenu() override;

  void set_skip_window_active_check_for_testing(bool skip) {
    skip_window_active_check_for_testing_ = skip;
  }

 private:
  friend class ProfileMenuViewExtensionsTest;
  friend class ProfileMenuViewSigninPendingTest;
  friend class ProfileMenuViewSignoutTest;
  friend class ProfileMenuViewSyncErrorButtonTest;
  friend class ProfileMenuInteractiveUiTest;

  Browser& browser() const { return *browser_; }

  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  // Callback invoked whenever the view is being closed.
  void OnClose();

  // Button/link actions.
  void OnProfileManagementButtonClicked();
  void OnManageGoogleAccountButtonClicked();
  void OnGuestProfileButtonClicked();
  void OnExitProfileButtonClicked();
  void OnSyncSettingsButtonClicked();
  void OnGoogleServicesSettingsButtonClicked();
  void OnAccountSettingsButtonClicked();
  void OnSyncErrorButtonClicked(syncer::SyncService::UserActionableError error);
  void OnPasskeyUnlockButtonClicked();
  void OnSigninButtonClicked(CoreAccountInfo account,
                             ActionableItem button_type,
                             signin_metrics::AccessPoint access_point);
  void OnSignoutButtonClicked();
  void OnOtherProfileSelected(const base::FilePath& profile_path);
  void OnAddNewProfileButtonClicked();
  void OnManageProfilesButtonClicked();
  void OnEditProfileButtonClicked();
  void OnAutofillSettingsButtonClicked();
  void OnYourSavedInfoSettingsButtonClicked();
  void OnBatchUploadButtonClicked(ActionableItem button_type);

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  // Prevents flaky tests by skipping the browser window active check within
  // `ProfileMenuView::OnClose`. Window active status is often unpredictable
  // during automated tests.
  bool skip_window_active_check_for_testing_ = false;

  // Helper methods for building the menu.
  void SetMenuTitleForAccessibility();
  void BuildGuestIdentity();
  void MaybeBuildBatchUploadButton();
  void BuildAutofillSettingsButton();
  void BuildCustomizeProfileButton();
  void MaybeBuildChromeAccountSettingsButtonWithSync();
  void MaybeBuildChromeAccountSettingsButton();
  void MaybeBuildGoogleServicesSettingsButton();
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

  const raw_ref<Browser> browser_;
  signin::ProfileMenuAvatarButtonPromoInfo promo_info_;
  // If the profile menu opening originated from a Promo on the AvatarButton.
  bool from_avatar_promo_;

  std::u16string menu_title_;
  std::u16string menu_subtitle_;

  // A profile switcher object needed if the user triggers opening other
  // profile in a web app.
  std::optional<WebAppProfileSwitcher> app_profile_switcher_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_H_

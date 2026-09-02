// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/isolated_mode_menu_view.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_anchor.h"

namespace {

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  if (auto* profile_manager = g_browser_process->profile_manager()) {
    return profile_manager->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(profile->GetPath());
  }
  return nullptr;
}

std::string GetPrimaryAccountEmail(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
        .email;
  }
  return std::string();
}

std::u16string GetSessionSubtitle(const std::string& email) {
  if (email.empty()) {
    return l10n_util::GetStringUTF16(IDS_ISOLATED_MODE_SESSION_TITLE);
  }
  return l10n_util::GetStringFUTF16(
      IDS_PROFILE_MENU_PROFILE_IDENTIFIER_WITH_SEPARATOR,
      l10n_util::GetStringUTF16(IDS_ISOLATED_MODE_SESSION_TITLE),
      base::UTF8ToUTF16(email));
}

}  // namespace

IsolatedModeMenuView::IsolatedModeMenuView(views::BubbleAnchor anchor_element,
                                           BrowserWindowInterface* browser)
    : ProfileMenuViewBase(anchor_element, browser) {
  CHECK(profile().IsEnterpriseIsolatedModeProfile());
  GetViewAccessibility().SetName(GetAccessibleWindowTitle(),
                                 ax::mojom::NameFrom::kAttribute);

  base::RecordAction(base::UserMetricsAction("IsolatedModeMenu_Show"));
}

IsolatedModeMenuView::~IsolatedModeMenuView() = default;

void IsolatedModeMenuView::BuildMenu() {
  int isolated_window_count =
      static_cast<int>(ProfileBrowserCollection::GetForProfile(&profile())
                           ->GetOffTheRecordBrowserCount());
  std::u16string close_button_title = l10n_util::GetPluralStringFUTF16(
      IDS_ISOLATED_MODE_PROFILE_MENU_CLOSE_BUTTON, isolated_window_count);

  Profile* original_profile = profile().GetOriginalProfile();
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(original_profile);
  std::string email = GetPrimaryAccountEmail(original_profile);

  IdentitySectionParams params;

  // Title: Profile display name (e.g. Gaia name, local name, or combination).
  if (entry) {
    params.title = GetProfileIdentifier(*entry);
  }

  // Subtitle: Isolated session title, formatted with email if available.
  params.subtitle = GetSessionSubtitle(email);

  // Header: Managed by organization banner.
  params.header_string =
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PROFILE_MANAGED_HEADER);
  params.header_image = ui::ImageModel::FromVectorIcon(
      GetManagedUiIcon(original_profile), ui::kColorIcon);
  params.header_action = base::BindRepeating(
      &IsolatedModeMenuView::OnProfileManagementButtonClicked,
      base::Unretained(this));

  // Avatar: Avatar image from the original profile.
  if (entry) {
    profiles::PlaceholderAvatarIconParams icon_params = {
        .has_padding = true, .has_background = false};
    params.profile_image = ui::ImageModel::FromImage(
        entry->GetAvatarIcon(kIdentityInfoImageSize,
                             /*use_high_res_file=*/true, icon_params));
  }

  SetProfileIdentityWithCallToAction(std::move(params));
  AddBottomMargin();

  // Exit button: Close all open isolated mode windows.
  const float icon_to_image_ratio =
      features::IsRoundedIconsEnabled() ? 1.3 : 1.0;
  AddFeatureButton(
      close_button_title,
      base::BindRepeating(&IsolatedModeMenuView::OnExitButtonClicked,
                          base::Unretained(this)),
      features::IsRoundedIconsEnabled() ? vector_icons::kCloseIcon
                                        : vector_icons::kCloseOldIcon,
      icon_to_image_ratio);
}

std::u16string IsolatedModeMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_ISOLATED_MODE_BUBBLE_ACCESSIBLE_TITLE,
      static_cast<int>(ProfileBrowserCollection::GetForProfile(&profile())
                           ->GetOffTheRecordBrowserCount()));
}

void IsolatedModeMenuView::OnProfileManagementButtonClicked() {
  OnActionableItemClicked(ActionableItem::kProfileManagementLabel);
  if (!perform_menu_actions()) {
    return;
  }
  Profile* original_profile = profile().GetOriginalProfile();
  ShowSingletonTab(original_profile, GetManagedUiUrl(original_profile));
}

void IsolatedModeMenuView::OnExitButtonClicked() {
  OnActionableItemClicked(ActionableItem::kExitProfileButton);
  base::RecordAction(base::UserMetricsAction("IsolatedModeMenu_ExitClicked"));
  chrome::CloseAllBrowsersWithIncognitoProfile(&profile());
}

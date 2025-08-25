// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"

IncognitoMenuView::IncognitoMenuView(ui::TrackedElement* anchor_element,
                                     Browser* browser)
    : ProfileMenuViewBase(anchor_element, browser) {
  CHECK(profile().IsIncognitoProfile());
  GetViewAccessibility().SetName(GetAccessibleWindowTitle(),
                                 ax::mojom::NameFrom::kAttribute);

  base::RecordAction(base::UserMetricsAction("IncognitoMenu_Show"));
}

IncognitoMenuView::~IncognitoMenuView() = default;

void IncognitoMenuView::BuildMenu() {
  int incognito_window_count =
      BrowserList::GetOffTheRecordBrowsersActiveForProfile(&profile());
  std::u16string close_button_title = l10n_util::GetPluralStringFUTF16(
      IDS_INCOGNITO_PROFILE_MENU_CLOSE_X_WINDOWS_BUTTON,
      incognito_window_count);
  IdentitySectionParams params;
  params.title = l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE);

  // Padded icon.
  params.profile_image_padding = std::nearbyint(kIdentityInfoImageSize * 0.25f);
  params.profile_image = ui::ImageModel::FromVectorIcon(
      kIncognitoRefreshMenuIcon, kColorAvatarButtonHighlightIncognitoForeground,
      kIdentityInfoImageSize - 2 * params.profile_image_padding);
  SetProfileIdentityWithCallToAction(std::move(params));
  AddBottomMargin();

  AddFeatureButton(close_button_title,
                   base::BindRepeating(&IncognitoMenuView::OnExitButtonClicked,
                                       base::Unretained(this)),
                   vector_icons::kCloseIcon);
}

std::u16string IncognitoMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE,
      BrowserList::GetOffTheRecordBrowsersActiveForProfile(&profile()));
}

void IncognitoMenuView::OnExitButtonClicked() {
  OnActionableItemClicked(ActionableItem::kExitProfileButton);
  base::RecordAction(base::UserMetricsAction("IncognitoMenu_ExitClicked"));
  // Skipping before-unload trigger to give incognito mode users a chance to
  // quickly close all incognito windows without needing to confirm closing the
  // open forms.
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      &profile(), /*on_close_success=*/base::DoNothing(),
      /*on_close_aborted=*/base::DoNothing(),
      /*skip_beforeunload=*/true);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/style/typography.h"

IncognitoMenuView::IncognitoMenuView(views::Button* anchor_button,
                                     Browser* browser)
    : ProfileMenuViewBase(anchor_button, browser) {
  DCHECK(browser->profile()->IsIncognitoProfile());
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::INCOGNITO_WINDOW_COUNT);

  base::RecordAction(base::UserMetricsAction("IncognitoMenu_Show"));
}

IncognitoMenuView::~IncognitoMenuView() = default;

void IncognitoMenuView::BuildMenu() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  int incognito_window_count =
      BrowserList::GetIncognitoSessionsActiveForProfile(browser()->profile());
  // The icon color is set to match the menu text, which guarantees sufficient
  // contrast and a consistent visual appearance.
  const SkColor icon_color = provider->GetTypographyProvider().GetColor(
      *this, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);

  if (!base::FeatureList::IsEnabled(features::kProfileMenuRevamp)) {
    auto incognito_icon = std::make_unique<views::ImageView>();
    incognito_icon->SetImage(
        gfx::CreateVectorIcon(kIncognitoProfileIcon, icon_color));

    AddMenuGroup(false /* add_separator */);
    CreateAndAddTitleCard(
        std::move(incognito_icon),
        l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE),
        incognito_window_count > 1
            ? l10n_util::GetPluralStringFUTF16(
                  IDS_INCOGNITO_WINDOW_COUNT_MESSAGE, incognito_window_count)
            : base::string16(),
        base::RepeatingClosure());

    AddMenuGroup();
    exit_button_ = CreateAndAddButton(
        gfx::CreateVectorIcon(kCloseAllIcon, 16, gfx::kChromeIconGrey),
        l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_CLOSE_BUTTON),
        base::BindRepeating(&IncognitoMenuView::OnExitButtonClicked,
                            base::Unretained(this)));
    return;
  }

  SetIdentityInfo(
      ColoredImageForMenu(kIncognitoProfileIcon, icon_color),
      /*badge=*/gfx::ImageSkia(),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE),
      incognito_window_count > 1
          ? l10n_util::GetPluralStringFUTF16(IDS_INCOGNITO_WINDOW_COUNT_MESSAGE,
                                             incognito_window_count)
          : base::string16());
  AddFeatureButton(
      ImageForMenu(kCloseAllIcon),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_CLOSE_BUTTON),
      base::BindRepeating(&IncognitoMenuView::OnExitButtonClicked,
                          base::Unretained(this)));
}

base::string16 IncognitoMenuView::GetAccessibleWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE,
      BrowserList::GetIncognitoSessionsActiveForProfile(browser()->profile()));
}

void IncognitoMenuView::OnExitButtonClicked() {
  RecordClick(ActionableItem::kExitProfileButton);
  // Skipping before-unload trigger to give incognito mode users a chance to
  // quickly close all incognito windows without needing to confirm closing the
  // open forms.
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      browser()->profile(), base::DoNothing(), base::DoNothing(),
      true /* skip_beforeunload */);
}

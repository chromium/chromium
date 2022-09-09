// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"

#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

ProfileMenuCoordinator::~ProfileMenuCoordinator() {
  // Forcefully close the Widget if it hasn't been closed by the time the
  // browser is torn down to avoid dangling references.
  if (IsShowing())
    bubble_tracker_.view()->GetWidget()->CloseNow();
}

void ProfileMenuCoordinator::Show(bool is_source_accelerator) {
  auto* avatar_toolbar_button =
      BrowserView::GetBrowserViewForBrowser(&GetBrowser())
          ->toolbar_button_provider()
          ->GetAvatarToolbarButton();

  // Do not show avatar bubble if there is no avatar menu button or the bubble
  // is already showing.
  if (!avatar_toolbar_button || IsShowing())
    return;

  auto& browser = GetBrowser();
  signin_ui_util::RecordProfileMenuViewShown(browser.profile());
  // Close any existing IPH bubble for the profile menu.
  browser.window()->CloseFeaturePromo(
      feature_engagement::kIPHProfileSwitchFeature);

  std::unique_ptr<ProfileMenuViewBase> bubble;
  if (browser.profile()->IsIncognitoProfile()) {
    bubble =
        std::make_unique<IncognitoMenuView>(avatar_toolbar_button, &browser);
  } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Note: on Ash, Guest Sessions have incognito profiles, and use
    // BUBBLE_VIEW_MODE_INCOGNITO.
    NOTREACHED() << "The profile menu is not implemented on Ash.";
#else
    bubble = std::make_unique<ProfileMenuView>(avatar_toolbar_button, &browser);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  auto* bubble_ptr = bubble.get();
  DCHECK_EQ(nullptr, bubble_tracker_.view());
  bubble_tracker_.SetView(bubble_ptr);

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  bubble_ptr->CreateAXWidgetObserver(widget);
  widget->Show();
  if (is_source_accelerator)
    bubble_ptr->FocusFirstProfileButton();
}

bool ProfileMenuCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}

ProfileMenuViewBase*
ProfileMenuCoordinator::GetProfileMenuViewBaseForTesting() {
  return IsShowing()
             ? views::AsViewClass<ProfileMenuViewBase>(bubble_tracker_.view())
             : nullptr;
}

ProfileMenuCoordinator::ProfileMenuCoordinator(Browser* browser)
    : BrowserUserData<ProfileMenuCoordinator>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(ProfileMenuCoordinator);

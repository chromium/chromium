// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"

#include "build/build_config.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"
#include "chrome/browser/chromeos/apps/intent_helper/common_apps_navigation_throttle.h"
#endif  //  defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
#include "chrome/browser/apps/intent_helper/mac_apps_navigation_throttle.h"
#endif  //  defined(OS_MACOSX)

namespace content {
class WebContents;
}

IntentPickerView::IntentPickerView(Browser* browser,
                                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate), browser_(browser) {}

IntentPickerView::~IntentPickerView() = default;

bool IntentPickerView::Update() {
  bool was_visible = GetVisible();

  SetVisible(ShouldShowIcon());

  if (was_visible && !GetVisible())
    IntentPickerBubbleView::CloseCurrentBubble();

  return was_visible != GetVisible();
}

void IntentPickerView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  DCHECK(ShouldShowIcon());
  content::WebContents* web_contents = GetWebContents();
  const GURL& url = chrome::GetURLToBookmark(web_contents);
#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kAppServiceIntentHandling)) {
    apps::CommonAppsNavigationThrottle::ShowIntentPickerBubble(
        web_contents, /*ui_auto_display_service=*/nullptr, url);
  } else {
    chromeos::ChromeOsAppsNavigationThrottle::ShowIntentPickerBubble(
        web_contents, /*ui_auto_display_service=*/nullptr, url);
  }
#elif defined(OS_MACOSX)
  apps::MacAppsNavigationThrottle::ShowIntentPickerBubble(
      web_contents, /*ui_auto_display_service=*/nullptr, url);
#else
  apps::AppsNavigationThrottle::ShowIntentPickerBubble(
      web_contents, /*ui_auto_display_service=*/nullptr, url);
#endif  //  defined(OS_CHROMEOS)
}

views::BubbleDialogDelegateView* IntentPickerView::GetBubble() const {
  return IntentPickerBubbleView::intent_picker_bubble();
}

bool IntentPickerView::IsIncognitoMode() const {
  DCHECK(browser_);

  return browser_->profile()->IsOffTheRecord();
}

bool IntentPickerView::ShouldShowIcon() const {
  if (IsIncognitoMode())
    return false;

  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  IntentPickerTabHelper* tab_helper =
      IntentPickerTabHelper::FromWebContents(web_contents);

  if (!tab_helper)
    return false;

  return tab_helper->should_show_icon();
}

const gfx::VectorIcon& IntentPickerView::GetVectorIcon() const {
  return vector_icons::kOpenInNewIcon;
}

base::string16 IntentPickerView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON);
}

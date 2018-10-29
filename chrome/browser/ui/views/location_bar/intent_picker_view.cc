// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"

#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/chromeos/arc/intent_helper/intent_picker_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

namespace content {
class WebContents;
}

IntentPickerView::IntentPickerView(Browser* browser,
                                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate), browser_(browser) {
  if (browser_) {
    intent_picker_controller_ =
        std::make_unique<arc::IntentPickerController>(browser_);
  }
}

IntentPickerView::~IntentPickerView() = default;

void IntentPickerView::SetVisible(bool visible) {
  // Besides changing visibility, make sure that we don't leave an opened bubble
  // when transitioning to !visible.
  if (!visible)
    IntentPickerBubbleView::CloseCurrentBubble();

  PageActionIconView::SetVisible(visible);
}

void IntentPickerView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (browser_ && !browser_->profile()->IsGuestSession() &&
      !IsIncognitoMode()) {
    SetVisible(true);
    content::WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    const GURL& url = chrome::GetURLToBookmark(web_contents);

    chromeos::AppsNavigationThrottle::ShowIntentPickerBubble(web_contents, url);
  } else {
    SetVisible(false);
  }
}

views::BubbleDialogDelegateView* IntentPickerView::GetBubble() const {
  return IntentPickerBubbleView::intent_picker_bubble();
}

bool IntentPickerView::IsIncognitoMode() {
  DCHECK(browser_);

  return browser_->profile()->IsOffTheRecord();
}

const gfx::VectorIcon& IntentPickerView::GetVectorIcon() const {
  return omnibox::kOpenInNewIcon;
}

base::string16 IntentPickerView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON);
}

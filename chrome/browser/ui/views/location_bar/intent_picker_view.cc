// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"

#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
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

namespace content {
class WebContents;
}

IntentPickerView::IntentPickerView(
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate),
      browser_(browser) {}

IntentPickerView::~IntentPickerView() = default;

void IntentPickerView::UpdateImpl() {
  bool was_visible = GetVisible();

  SetVisible(ShouldShowIcon());

  if (was_visible && !GetVisible())
    IntentPickerBubbleView::CloseCurrentBubble();
}

void IntentPickerView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  DCHECK(ShouldShowIcon());
  content::WebContents* web_contents = GetWebContents();
  const GURL& url = chrome::GetURLToBookmark(web_contents);
  apps::ShowIntentPickerBubble(web_contents, url);
}

views::BubbleDialogDelegate* IntentPickerView::GetBubble() const {
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

const char* IntentPickerView::GetClassName() const {
  return "IntentPickerView";
}

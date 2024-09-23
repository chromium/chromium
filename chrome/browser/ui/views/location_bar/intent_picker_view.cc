// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

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
                         page_action_icon_delegate,
                         "IntentPicker"),
      browser_(browser) {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON));
}

IntentPickerView::~IntentPickerView() = default;

void IntentPickerView::UpdateImpl() {
  bool was_visible = GetVisible();

  SetVisible(GetShowIcon());

  if (was_visible && !GetVisible())
    IntentPickerBubbleView::CloseCurrentBubble();
}

void IntentPickerView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  DCHECK(GetShowIcon());
  content::WebContents* web_contents = GetWebContents();
  CHECK(web_contents);
  const GURL& url = chrome::GetURLToBookmark(web_contents);
  IntentPickerTabHelper* intent_picker_tab_helper =
      IntentPickerTabHelper::FromWebContents(web_contents);
  CHECK(intent_picker_tab_helper);
  intent_picker_tab_helper->ShowIntentPickerBubbleOrLaunchApp(url);
}

views::BubbleDialogDelegate* IntentPickerView::GetBubble() const {
  return IntentPickerBubbleView::intent_picker_bubble();
}

bool IntentPickerView::GetShowIcon() const {
  if (browser_->profile()->IsOffTheRecord())
    return false;

  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  IntentPickerTabHelper* tab_helper =
      IntentPickerTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->should_show_icon();
}

const gfx::VectorIcon& IntentPickerView::GetVectorIcon() const {
  return kOpenInNewChromeRefreshIcon;
}

BEGIN_METADATA(IntentPickerView)
ADD_READONLY_PROPERTY_METADATA(bool, ShowIcon)
END_METADATA

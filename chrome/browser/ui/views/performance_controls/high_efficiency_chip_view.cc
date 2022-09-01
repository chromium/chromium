// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

HighEfficiencyChipView::HighEfficiencyChipView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {
  SetProperty(views::kElementIdentifierKey, kHighEfficiencyChipElementId);
}

HighEfficiencyChipView::~HighEfficiencyChipView() = default;

void HighEfficiencyChipView::UpdateImpl() {
  content::WebContents* const web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  TabDiscardTabHelper* const tab_helper =
      TabDiscardTabHelper::FromWebContents(web_contents);
  SetVisible(tab_helper->IsChipVisible());
}

void HighEfficiencyChipView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

bool HighEfficiencyChipView::IsBubbleShowing() const {
  return false;
}

const gfx::VectorIcon& HighEfficiencyChipView::GetVectorIcon() const {
  return kHighEfficiencyIcon;
}

views::BubbleDialogDelegate* HighEfficiencyChipView::GetBubble() const {
  return nullptr;
}

std::u16string HighEfficiencyChipView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_CHIP_ACCNAME);
}

BEGIN_METADATA(HighEfficiencyChipView, PageActionIconView)
END_METADATA

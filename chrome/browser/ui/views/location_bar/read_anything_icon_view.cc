// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/read_anything_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

ReadAnythingIconView::ReadAnythingIconView(
    CommandUpdater* command_updater,
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SHOW_READING_MODE_SIDE_PANEL,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "ReadAnythingIcon",
                         true),
      browser_(browser) {
  DCHECK(browser_);

  SetActive(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE));
}

ReadAnythingIconView::~ReadAnythingIconView() = default;

void ReadAnythingIconView::UpdateImpl() {
  // TODO(crbug.com/1266555): Only show icon when the active tab is distillable.
  SetVisible(true);
}

void ReadAnythingIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  ShowReadAnythingSidePanel(browser_,
                            SidePanelOpenTrigger::kReadAnythingOmniboxIcon);
  // TODO(crbug.com/1266555): Icon should disappear and never be shown again for
  // this tab.
}

views::BubbleDialogDelegate* ReadAnythingIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& ReadAnythingIconView::GetVectorIcon() const {
  return kMenuBookChromeRefreshIcon;
}

bool ReadAnythingIconView::ShouldShowLabel() const {
  // TODO(crbug.com/1266555): Only show label the first 3 times.
  return true;
}

BEGIN_METADATA(ReadAnythingIconView, PageActionIconView)
END_METADATA

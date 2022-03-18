// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view_class_properties.h"

SideSearchIconView::SideSearchIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate),
      browser_(browser),
      default_search_icon_source_(
          browser,
          base::BindRepeating(&SideSearchIconView::UpdateIconImage,
                              base::Unretained(this))) {
  image()->SetFlipCanvasOnPaintForRTLUI(false);
  SetProperty(views::kElementIdentifierKey, kSideSearchButtonElementId);
  SetVisible(false);
}

SideSearchIconView::~SideSearchIconView() = default;

void SideSearchIconView::UpdateImpl() {
  content::WebContents* active_contents = GetWebContents();
  if (!active_contents)
    return;

  if (active_contents->IsCrashed()) {
    SetVisible(false);
    return;
  }

  // Only show the page action button if the side panel is showable for this
  // active web contents and is not currently toggled open.
  // TODO(tluk): Setup conditions for `AnimateIn()`.
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);
  const bool should_show =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation() &&
      !tab_contents_helper->toggled_open();
  SetVisible(should_show);
}

void SideSearchIconView::OnExecuting(PageActionIconView::ExecuteSource source) {
  auto* side_search_browser_controller =
      BrowserView::GetBrowserViewForBrowser(browser_)->side_search_controller();
  side_search_browser_controller->ToggleSidePanel();
}

views::BubbleDialogDelegate* SideSearchIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& SideSearchIconView::GetVectorIcon() const {
  return gfx::kNoneIcon;
}

ui::ImageModel SideSearchIconView::GetSizedIconImage(int size) const {
  return default_search_icon_source_.GetSizedIconImage(size);
}

std::u16string SideSearchIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED);
}

BEGIN_METADATA(SideSearchIconView, PageActionIconView)
END_METADATA

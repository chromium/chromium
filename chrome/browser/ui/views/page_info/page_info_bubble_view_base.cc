// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"

#include <string>

#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "components/page_info/page_info_ui.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/buildflags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// NOTE(jdonnelly): The following two process-wide variables assume that there's
// never more than one page info bubble shown and that it's associated with the
// current window. If this assumption fails in the future, we'll need to return
// a weak pointer from ShowBubble so callers can associate it with the current
// window (or other context) and check if the bubble they care about is showing.
PageInfoBubbleViewBase::BubbleType g_shown_bubble_type =
    PageInfoBubbleViewBase::BUBBLE_NONE;
PageInfoBubbleViewBase* g_page_info_bubble = nullptr;

}  // namespace

// static
PageInfoBubbleViewBase::BubbleType
PageInfoBubbleViewBase::GetShownBubbleType() {
  return g_shown_bubble_type;
}

// static
views::BubbleDialogDelegateView*
PageInfoBubbleViewBase::GetPageInfoBubbleForTesting() {
  return g_page_info_bubble;
}

PageInfoBubbleViewBase::PageInfoBubbleViewBase(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    PageInfoBubbleViewBase::BubbleType type,
    content::WebContents* web_contents)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::DIALOG_SHADOW,
                               /*autosize=*/false),
      content::WebContentsObserver(web_contents) {
  g_shown_bubble_type = type;
  g_page_info_bubble = this;

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);

  set_parent_window(parent_window);
  if (!anchor_view)
    SetAnchorRect(anchor_rect);
}

void PageInfoBubbleViewBase::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  g_shown_bubble_type = BUBBLE_NONE;
  g_page_info_bubble = nullptr;
}

void PageInfoBubbleViewBase::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    GetWidget()->Close();
  }
}

void PageInfoBubbleViewBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    GetWidget()->Close();
}

void PageInfoBubbleViewBase::PrimaryPageChanged(content::Page& page) {
  GetWidget()->Close();
}

void PageInfoBubbleViewBase::DidChangeVisibleSecurityState() {
  // Subclasses may update instead, but this the only safe general option.
  GetWidget()->Close();
}

void PageInfoBubbleViewBase::WebContentsDestroyed() {
  GetWidget()->Close();
}

BEGIN_METADATA(PageInfoBubbleViewBase)
END_METADATA

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/zoom_view.h"

#include "base/i18n/number_formatting.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/zoom/zoom_controller.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"

ZoomView::ZoomView(IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                   PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "Zoom"),
      icon_(&kZoomMinusIcon) {
  SetVisible(false);
  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_TOOLTIP_ZOOM, base::FormatPercent(current_zoom_percent_)));
}

ZoomView::~ZoomView() {}

void ZoomView::UpdateImpl() {
  ZoomChangedForActiveTab(false);
}

bool ZoomView::ShouldBeVisible(bool can_show_bubble) const {
  if (delegate()->ShouldHidePageActionIcons())
    return false;

  if (can_show_bubble)
    return true;

  if (HasAssociatedBubble())
    return true;

  DCHECK(GetWebContents());
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(GetWebContents());
  return zoom_controller && !zoom_controller->IsAtDefaultZoom();
}

bool ZoomView::HasAssociatedBubble() const {
  if (!GetBubble())
    return false;

  // Bubbles may be hosted in their own widget so use their anchor view as a
  // more reliable way of determining whether this icon belongs to the same
  // browser window.
  if (!GetBubble()->GetAnchorView())
    return false;
  return GetBubble()->GetAnchorView()->GetWidget() == GetWidget();
}

void ZoomView::ZoomChangedForActiveTab(bool can_show_bubble) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  if (ShouldBeVisible(can_show_bubble)) {
    zoom::ZoomController* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents);
    current_zoom_percent_ = zoom_controller->GetZoomPercent();

    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_TOOLTIP_ZOOM, base::FormatPercent(current_zoom_percent_)));

    // The icon is hidden when the zoom level is default.
      icon_ =
          zoom_controller && zoom_controller->GetZoomRelativeToDefault() ==
                                 zoom::ZoomController::ZOOM_BELOW_DEFAULT_ZOOM
              ? &kZoomMinusChromeRefreshIcon
              : &kZoomPlusChromeRefreshIcon;
    UpdateIconImage();

    // Visibility must be enabled before the bubble is shown to ensure the
    // bubble anchors correctly.
    SetVisible(true);

    if (can_show_bubble) {
      ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
    } else {
      ZoomBubbleView::RefreshBubbleIfShowing(web_contents);
    }
  } else {
    // Close the bubble first to ensure focus is not lost when SetVisible(false)
    // is called. See crbug.com/913829.
    if (HasAssociatedBubble())
      ZoomBubbleView::CloseCurrentBubble();
    SetVisible(false);
  }
}

void ZoomView::OnExecuting(PageActionIconView::ExecuteSource source) {
  ZoomBubbleView::ShowBubble(GetWebContents(), ZoomBubbleView::USER_GESTURE);
}

views::BubbleDialogDelegate* ZoomView::GetBubble() const {
  return ZoomBubbleView::GetZoomBubble();
}

const gfx::VectorIcon& ZoomView::GetVectorIcon() const {
  return *icon_;
}

BEGIN_METADATA(ZoomView)
END_METADATA

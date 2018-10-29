// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/zoom_view.h"

#include "base/i18n/number_formatting.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/zoom/zoom_controller.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"

ZoomView::ZoomView(LocationBarView::Delegate* location_bar_delegate,
                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate),
      location_bar_delegate_(location_bar_delegate),
      icon_(&kZoomMinusIcon) {
  SetVisible(false);
}

ZoomView::~ZoomView() {}

bool ZoomView::Update() {
  bool was_visible = visible();
  ZoomChangedForActiveTab(false);
  return visible() != was_visible;
}

bool ZoomView::ShouldBeVisible(bool can_show_bubble) const {
  if (location_bar_delegate_ &&
      location_bar_delegate_->GetToolbarModel()->input_in_progress()) {
    return false;
  }

  if (can_show_bubble)
    return true;

  if (ZoomBubbleView::GetZoomBubble())
    return true;

  DCHECK(GetWebContents());
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(GetWebContents());
  return zoom_controller && !zoom_controller->IsAtDefaultZoom();
}

void ZoomView::ZoomChangedForActiveTab(bool can_show_bubble) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  if (ShouldBeVisible(can_show_bubble)) {
    zoom::ZoomController* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents);
    current_zoom_percent_ = zoom_controller->GetZoomPercent();

    // The icon is hidden when the zoom level is default.
    icon_ = zoom_controller && zoom_controller->GetZoomRelativeToDefault() ==
                                   zoom::ZoomController::ZOOM_BELOW_DEFAULT_ZOOM
                ? &kZoomMinusIcon
                : &kZoomPlusIcon;
    if (GetNativeTheme())
      UpdateIconImage();

    // Visibility must be enabled before the bubble is shown to ensure the
    // bubble anchors correctly.
    SetVisible(true);

    if (can_show_bubble) {
      ZoomBubbleView::ShowBubble(web_contents, gfx::Point(),
                                 ZoomBubbleView::AUTOMATIC);
    } else {
      ZoomBubbleView::RefreshBubbleIfShowing(web_contents);
    }
  } else {
    SetVisible(false);
    ZoomBubbleView::CloseCurrentBubble();
  }
}

void ZoomView::OnExecuting(PageActionIconView::ExecuteSource source) {
  ZoomBubbleView::ShowBubble(GetWebContents(), gfx::Point(),
                             ZoomBubbleView::USER_GESTURE);
}

views::BubbleDialogDelegateView* ZoomView::GetBubble() const {
  return ZoomBubbleView::GetZoomBubble();
}

const gfx::VectorIcon& ZoomView::GetVectorIcon() const {
  return *icon_;
}

base::string16 ZoomView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringFUTF16(IDS_TOOLTIP_ZOOM,
                                    base::FormatPercent(current_zoom_percent_));
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace zoom {

ZoomViewController::ZoomViewController(tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  DCHECK(IsPageActionMigrated(PageActionIconType::kZoom));
}

ZoomViewController::~ZoomViewController() = default;

void ZoomViewController::UpdatePageActionIconAndBubbleVisibility(
    bool prefer_to_show_bubble,
    bool from_user_gesture) {
  UpdatePageActionIcon();
  UpdateBubbleVisibility(prefer_to_show_bubble, from_user_gesture);
}

void ZoomViewController::UpdatePageActionIcon() {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(GetWebContents());
  CHECK(zoom_controller);

  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);

  // Update the tooltip with the current zoom percentage.
  page_action_controller->OverrideTooltip(
      kActionZoomNormal,
      l10n_util::GetStringFUTF16(
          IDS_TOOLTIP_ZOOM,
          base::FormatPercent(zoom_controller->GetZoomPercent())));

  switch (zoom_controller->GetZoomRelativeToDefault()) {
    case ZoomController::ZOOM_BELOW_DEFAULT_ZOOM:
      page_action_controller->OverrideImage(
          kActionZoomNormal,
          ui::ImageModel::FromVectorIcon(kZoomMinusChromeRefreshIcon));
      break;
    case ZoomController::ZOOM_AT_DEFAULT_ZOOM:
      // Default and above share the “zoom plus” icon for simplicity.
    case ZoomController::ZOOM_ABOVE_DEFAULT_ZOOM:
      page_action_controller->OverrideImage(
          kActionZoomNormal,
          ui::ImageModel::FromVectorIcon(kZoomPlusChromeRefreshIcon));
      break;
    default:
      NOTREACHED();
  }

  // Show or hide the page action icon. Hide it if at default zoom and no
  // bubble.
  const bool is_at_default_zoom = zoom_controller->IsAtDefaultZoom();
  if (is_at_default_zoom && !IsBubbleVisible()) {
    page_action_controller->Hide(kActionZoomNormal);
  } else {
    page_action_controller->Show(kActionZoomNormal);
  }
}

void ZoomViewController::UpdateBubbleVisibility(bool prefer_to_show_bubble,
                                                bool from_user_gesture) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(GetWebContents());
  CHECK(zoom_controller);

  const bool is_at_default_zoom = zoom_controller->IsAtDefaultZoom();
  const bool can_bubble_be_visible =
      CanBubbleBeVisible(prefer_to_show_bubble, is_at_default_zoom);
  if (can_bubble_be_visible) {
    if (prefer_to_show_bubble) {
      ZoomBubbleView::ShowBubble(
          GetWebContents(), from_user_gesture ? ZoomBubbleView::USER_GESTURE
                                              : ZoomBubbleView::AUTOMATIC);
    } else {
      ZoomBubbleView::RefreshBubbleIfShowing(GetWebContents());
    }
  } else {
    if (IsBubbleVisible()) {
      ZoomBubbleView::CloseCurrentBubble();
    }
  }
}

bool ZoomViewController::IsBubbleVisible() const {
  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionZoomNormal, tab_interface_->GetBrowserWindowInterface()
                             ->GetActions()
                             ->root_action_item());
  return action_item->GetIsShowingBubble();
}

bool ZoomViewController::CanBubbleBeVisible(bool prefer_to_show_bubble,
                                            bool is_zoom_at_default) const {
  // The zoom bubble should be visible if either:
  // 1. The caller prefers to show the bubble (`prefer_to_show_bubble`), or
  // 2. The bubble is already visible (`IsBubbleVisible()`).
  //
  // If neither of these is true, we only show the bubble if the zoom level
  // is not at the default (`!is_zoom_at_default`), indicating that zoom is
  // active.
  return (prefer_to_show_bubble || IsBubbleVisible()) ? true
                                                      : !is_zoom_at_default;
}

content::WebContents* ZoomViewController::GetWebContents() const {
  return tab_interface_->GetContents();
}

}  // namespace zoom

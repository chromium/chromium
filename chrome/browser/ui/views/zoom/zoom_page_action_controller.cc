// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/zoom/zoom_page_action_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace zoom {

ZoomPageActionController::ZoomPageActionController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  CHECK(base::FeatureList::IsEnabled(features::kPageActionsMigration));

  will_discard_contents_subscription_ =
      tab_interface.RegisterWillDiscardContents(
          base::BindRepeating(&ZoomPageActionController::WillDiscardContents,
                              base::Unretained(this)));
  if (auto* zoom_controller = zoom::ZoomController::FromWebContents(
          tab_interface_->GetContents())) {
    zoom_observation_.Observe(zoom_controller);
    UpdatePageAction();
  }
}

ZoomPageActionController::~ZoomPageActionController() {
  zoom_observation_.Reset();
}

void ZoomPageActionController::UpdatePageAction() {
  CHECK(zoom_observation_.IsObserving());

  ZoomController* zoom_controller = zoom_observation_.GetSource();

  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);

  page_action_controller->OverrideTooltip(
      kActionZoomNormal,
      l10n_util::GetStringFUTF16(
          IDS_TOOLTIP_ZOOM,
          base::FormatPercent(zoom_controller->GetZoomPercent())));
  ZoomController::RelativeZoom zoom_relative_to_default =
      zoom_controller->GetZoomRelativeToDefault();
  switch (zoom_relative_to_default) {
    case ZoomController::ZOOM_BELOW_DEFAULT_ZOOM:
      page_action_controller->OverrideImage(
          kActionZoomNormal,
          ui::ImageModel::FromVectorIcon(kZoomMinusChromeRefreshIcon));
      page_action_controller->Show(kActionZoomNormal);
      break;
    case ZoomController::ZOOM_AT_DEFAULT_ZOOM:
      // Don't need to override the image since the page action will be hidden.
      page_action_controller->Hide(kActionZoomNormal);
      break;
    case ZoomController::ZOOM_ABOVE_DEFAULT_ZOOM:
      page_action_controller->OverrideImage(
          kActionZoomNormal,
          ui::ImageModel::FromVectorIcon(kZoomPlusChromeRefreshIcon));
      page_action_controller->Show(kActionZoomNormal);
      break;
    default:
      NOTREACHED();
  }
}

void ZoomPageActionController::OnZoomControllerDestroyed(
    ZoomController* source) {
  // WillDiscardContents() will takes care of removing the observer.
}

void ZoomPageActionController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_content,
    content::WebContents* new_content) {
  zoom_observation_.Reset();

  CHECK_EQ(tab_interface_->GetContents(), new_content);

  if (auto* zoom_controller =
          zoom::ZoomController::FromWebContents(new_content)) {
    zoom_observation_.Observe(zoom_controller);
  }
}

void ZoomPageActionController::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& data) {
  CHECK_EQ(data.web_contents, tab_interface_->GetContents());

  UpdatePageAction();
}

}  // namespace zoom

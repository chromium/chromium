// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"

#include "components/autofill/core/browser/ui/popup_hiding_reasons.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AutofillPopupHideHelper::AutofillPopupHideHelper(
    content::WebContents* web_contents,
    HidingParams hiding_params,
    HidingCallback hiding_callback)
    : content::WebContentsObserver(web_contents),
      hiding_params_(hiding_params) {
  hiding_callback_ = std::move(hiding_callback);
#if !BUILDFLAG(IS_ANDROID)
  // There may not always be a ZoomController, e.g., in tests.
  if (auto* zoom_controller =
          zoom::ZoomController::FromWebContents(web_contents)) {
    zoom_observation_.Observe(zoom_controller);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

AutofillPopupHideHelper::~AutofillPopupHideHelper() = default;

void AutofillPopupHideHelper::WebContentsDestroyed() {
  hiding_callback_.Run(PopupHidingReason::kTabGone);
}

void AutofillPopupHideHelper::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  hiding_callback_.Run(PopupHidingReason::kFocusChanged);
}

void AutofillPopupHideHelper::PrimaryMainFrameWasResized(bool width_changed) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    // Ignore virtual keyboard showing and hiding a strip of suggestions.
    if (!width_changed) {
      return;
    }
  }
  hiding_callback_.Run(PopupHidingReason::kWidgetChanged);
}

void AutofillPopupHideHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    hiding_callback_.Run(PopupHidingReason::kTabGone);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void AutofillPopupHideHelper::OnZoomControllerDestroyed(
    zoom::ZoomController* source) {
  zoom_observation_.Reset();
}

void AutofillPopupHideHelper::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  hiding_callback_.Run(PopupHidingReason::kContentAreaMoved);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill

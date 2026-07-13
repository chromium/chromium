// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace autofill {

AutofillPopupHideHelper::AutofillPopupHideHelper(
    content::WebContents* web_contents,
    content::GlobalRenderFrameHostId rfh_id,
    HidingParams hiding_params,
    HidingCallback hiding_callback,
    PictureInPictureDetectionCallback pip_detection_callback)
    : content::WebContentsObserver(web_contents),
      hiding_params_(std::move(hiding_params)),
      hiding_callback_(std::move(hiding_callback)),
      pip_detection_callback_(std::move(pip_detection_callback)),
      rfh_id_(rfh_id) {
#if !BUILDFLAG(IS_ANDROID)
  // There may not always be a ZoomController, e.g., in tests.
  if (auto* zoom_controller =
          zoom::ZoomController::FromWebContents(web_contents)) {
    zoom_observation_.Observe(zoom_controller);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  picture_in_picture_window_observation_.Observe(
      PictureInPictureWindowManager::GetInstance());
}

AutofillPopupHideHelper::~AutofillPopupHideHelper() = default;

void AutofillPopupHideHelper::WebContentsDestroyed() {
  hiding_callback_.Run(SuggestionHidingReason::kTabGone);
}

void AutofillPopupHideHelper::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  if (hiding_params_.hide_on_web_contents_lost_focus) {
    hiding_callback_.Run(SuggestionHidingReason::kFocusChanged);
  }
}

void AutofillPopupHideHelper::PrimaryMainFrameWasResized(bool width_changed) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    // Ignore virtual keyboard showing and hiding a strip of suggestions.
    return;
  }
  hiding_callback_.Run(SuggestionHidingReason::kWidgetChanged);
}

void AutofillPopupHideHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    hiding_callback_.Run(SuggestionHidingReason::kTabGone);
  }
}

void AutofillPopupHideHelper::RenderFrameHostStateChanged(
    content::RenderFrameHost* rfh,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  auto should_hide_popup = [](content::RenderFrameHost::LifecycleState state) {
    switch (state) {
      case content::RenderFrameHost::LifecycleState::kActive:
        return false;
      case content::RenderFrameHost::LifecycleState::kPendingCommit:
      case content::RenderFrameHost::LifecycleState::kPrerendering:
      case content::RenderFrameHost::LifecycleState::kInBackForwardCache:
      case content::RenderFrameHost::LifecycleState::kPendingDeletion:
        return true;
    }
    NOTREACHED();
  };

  // If the popup menu has been triggered from within an iframe and that frame
  // is deleted, hide the popup. This is necessary because the popup may
  // actually be shown by the `AutofillExternalDelegate` of an ancestor frame,
  // which is not notified about `rfh`'s destruction and therefore won't close
  // the popup.
  if (rfh_id_ == rfh->GetGlobalId() && should_hide_popup(new_state)) {
    hiding_callback_.Run(SuggestionHidingReason::kRendererEvent);
  }
}

void AutofillPopupHideHelper::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  // RenderFrameHostStateChanged() is not called when on FrameTree::Shutdown():
  // crbug.com/40693086.
  // For the primary frame tree, this is caught by WebContentsDestroyed(), but
  // for embedded frame trees we observe RenderFrameDeleted() to compensate for
  // the missing RenderFrameHostStateChanged().
  if (rfh_id_ == rfh->GetGlobalId()) {
    hiding_callback_.Run(SuggestionHidingReason::kRendererEvent);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void AutofillPopupHideHelper::OnZoomControllerDestroyed(
    zoom::ZoomController* source) {
  zoom_observation_.Reset();
}

void AutofillPopupHideHelper::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  // The ZoomController broadcasts OnZoomChanged on every navigation (including
  // same-document URL changes). To prevent closing the popup when the content
  // hasn't actually moved, ignore events where the zoom level is unchanged.
  if (blink::ZoomValuesEqual(data.old_zoom_level, data.new_zoom_level)) {
    return;
  }
  hiding_callback_.Run(SuggestionHidingReason::kContentAreaMoved);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AutofillPopupHideHelper::OnEnterPictureInPicture() {
  if (pip_detection_callback_.Run()) {
    hiding_callback_.Run(
        SuggestionHidingReason::kOverlappingWithPictureInPictureWindow);
  }
}

}  // namespace autofill

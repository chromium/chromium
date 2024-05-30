// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_HIDE_HELPER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_HIDE_HELPER_H_

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/zoom/zoom_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class NavigationHandle;
class RenderFrameHost;
class RenderWidgetHost;
enum class Visibility;
class WebContents;
}  // namespace content

namespace autofill {

enum class SuggestionHidingReason;

// AutofillPopupHideHelper is a class which detects events that should hide an
// Autofill Popup or any class that should have the same hiding behavior. The
// popup passes a `HidingCallback` in the constructor of the hiding helper,
// which will be called when a hiding event occurs. Note that some hiding events
// cannot be observed by this class because they are specific to the renderer,
// to suggestions, etc.
class AutofillPopupHideHelper : public content::WebContentsObserver,
                                public PictureInPictureWindowManager::Observer
#if !BUILDFLAG(IS_ANDROID)
    ,
                                public zoom::ZoomObserver
#endif  // !BUILDFLAG(IS_ANDROID)
{
 public:
  // This is a `RepeatingCallback` because multiple hiding events can occur at
  // the same time.
  using HidingCallback = base::RepeatingCallback<void(SuggestionHidingReason)>;
  using PictureInPictureDetectionCallback = base::RepeatingCallback<bool()>;

  // This struct configures what type of events the helper should call the
  // `hiding_callback_`.
  struct HidingParams {
    bool hide_on_web_contents_lost_focus = true;
  };

  AutofillPopupHideHelper(
      content::WebContents* web_contents,
      content::GlobalRenderFrameHostId rfh_id,
      HidingParams hiding_params,
      HidingCallback hiding_callback,
      PictureInPictureDetectionCallback pip_detection_callback);

  AutofillPopupHideHelper(const AutofillPopupHideHelper&) = delete;
  AutofillPopupHideHelper& operator=(const AutofillPopupHideHelper&) = delete;
  ~AutofillPopupHideHelper() override;

 private:
  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

#if !BUILDFLAG(IS_ANDROID)
  // ZoomObserver:
  void OnZoomControllerDestroyed(zoom::ZoomController* source) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif

  // PictureInPictureWindowManager::Observer
  void OnEnterPictureInPicture() override;

  const HidingParams hiding_params_;
  const HidingCallback hiding_callback_;
  // Returns true if the popup overlaps with a picture in picture window. It is
  // called inside `OnEnterPictureInPicture()`.
  const PictureInPictureDetectionCallback pip_detection_callback_;
  // ID for the focused frame.
  content::GlobalRenderFrameHostId rfh_id_;

#if !BUILDFLAG(IS_ANDROID)
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};
#endif

  // Observer needed to check autofill popup overlap with picture-in-picture
  // window. It is guaranteed that there can only be one
  // PictureInPictureWindowManager per Chrome instance, therefore, it is also
  // guaranteed that PictureInPictureWindowManager would outlive its observers.
  base::ScopedObservation<PictureInPictureWindowManager,
                          PictureInPictureWindowManager::Observer>
      picture_in_picture_window_observation_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_HIDE_HELPER_H_

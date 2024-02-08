// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_HIDE_HELPER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_HIDE_HELPER_H_

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#if !BUILDFLAG(IS_ANDROID)
#include "components/zoom/zoom_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class RenderWidgetHost;
enum class Visibility;
class WebContents;
}  // namespace content

namespace autofill {

enum class PopupHidingReason;

// AutofillPopupHideHelper is a class which detects events that should hide an
// Autofill Popup or any class that should have the same hiding behavior. The
// popup passes a `HidingCallback` in the constructor of the hiding helper,
// which will be called when a hiding event occurs. Note that some hiding events
// cannot be observed by this class because they are specific to the renderer,
// to suggestions, etc.
class AutofillPopupHideHelper : public content::WebContentsObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
                                public zoom::ZoomObserver
#endif  // !BUILDFLAG(IS_ANDROID)
{
 public:
  // This is a `RepeatingCallback` because multiple hiding events can occur at
  // the same time.
  using HidingCallback = base::RepeatingCallback<void(PopupHidingReason)>;

  // This struct configures what type of events the helper should call the
  // `hiding_callback_`.
  struct HidingParams {
    // TODO(b/320632147): Add parameters.
  };

  AutofillPopupHideHelper(content::WebContents* web_contents,
                          HidingParams hiding_params,
                          HidingCallback hiding_callback);
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

#if !BUILDFLAG(IS_ANDROID)
  // ZoomObserver:
  void OnZoomControllerDestroyed(zoom::ZoomController* source) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif

  HidingCallback hiding_callback_;
  HidingParams hiding_params_;

#if !BUILDFLAG(IS_ANDROID)
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};
#endif
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_HIDE_HELPER_H_

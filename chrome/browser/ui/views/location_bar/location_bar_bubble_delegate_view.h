// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_BUBBLE_DELEGATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_BUBBLE_DELEGATE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/metadata/view_factory.h"

namespace content {
class WebContents;
}  // namespace content

// Base class for bubbles that are shown from location bar icons. The bubble
// will automatically close when the browser transitions in or out of fullscreen
// mode.
// TODO(crbug.com/40551360): Move to chrome/browser/ui/views/page_action/.
class LocationBarBubbleDelegateView : public views::BubbleDialogDelegateView,
                                      public FullscreenObserver,
                                      public content::WebContentsObserver {
  METADATA_HEADER(LocationBarBubbleDelegateView,
                  views::BubbleDialogDelegateView)

 public:
  enum DisplayReason {
    // The bubble appears as a direct result of a user action (clicking on the
    // location bar icon).
    USER_GESTURE,

    // The bubble appears spontaneously over the course of the user's
    // interaction with Chrome (e.g. due to some change in the feature's
    // status).
    AUTOMATIC,
  };

  // Constructs LocationBarBubbleDelegateView. Anchors the bubble to
  // |anchor_view|. If |anchor_view| is nullptr, the bubble is anchored at
  // (0,0).
  // Registers with a fullscreen controller identified by |web_contents| to
  // close the bubble if the fullscreen state changes.
  LocationBarBubbleDelegateView(views::View* anchor_view,
                                content::WebContents* web_contents,
                                bool autosize = false);

  LocationBarBubbleDelegateView(const LocationBarBubbleDelegateView&) = delete;
  LocationBarBubbleDelegateView& operator=(
      const LocationBarBubbleDelegateView&) = delete;
  ~LocationBarBubbleDelegateView() override;

  // Displays the bubble with appearance and behavior tailored for |reason|.
  // If |allow_refocus_alert| is set to true (default), inactive bubbles will
  // have an additional screen reader alert instructing the user to use a
  // hotkey combination to focus the bubble; if it's set to false, no additional
  // alert is provided (use false for transient bubbles to avoid confusing the
  // user).
  void ShowForReason(DisplayReason reason, bool allow_refocus_alert = true);

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // views::BubbleDialogDelegateView:
  gfx::Rect GetAnchorBoundsInScreen() const override;

  // If the bubble is not anchored to a view, places the bubble in the top right
  // (left in RTL) of the |screen_bounds| that contain web contents's browser
  // window. Because the positioning is based on the size of the bubble, this
  // must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

 protected:
  // The class listens for WebContentsView events and closes the bubble. Useful
  // for bubbles that do not start out focused but need to close when the user
  // interacts with the web view.
  class WebContentMouseHandler : public ui::EventObserver {
   public:
    WebContentMouseHandler(LocationBarBubbleDelegateView* bubble,
                           content::WebContents* web_contents);

    WebContentMouseHandler(const WebContentMouseHandler&) = delete;
    WebContentMouseHandler& operator=(const WebContentMouseHandler&) = delete;

    ~WebContentMouseHandler() override;

    // ui::EventObserver:
    void OnEvent(const ui::Event& event) override;

   private:
    raw_ptr<LocationBarBubbleDelegateView> bubble_;
    raw_ptr<content::WebContents> web_contents_;
    std::unique_ptr<views::EventMonitor> event_monitor_;
  };

  // Closes the bubble.
  virtual void CloseBubble();

  void SetCloseOnMainFrameOriginNavigation(bool close);
  bool GetCloseOnMainFrameOriginNavigation() const;

 private:
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observation_{this};

  // Use to track down potential UaF. See https://crbug.com/1304280. Remove this
  // code when issue is fixed.
  base::WeakPtr<FullscreenController> fullscreen_controller_;

  // A flag controlling bubble closure when the main frame navigates to a
  // different origin.
  bool close_on_main_frame_origin_navigation_ = false;

  DisplayReason display_reason_ = AUTOMATIC;
};

BEGIN_VIEW_BUILDER(,
                   LocationBarBubbleDelegateView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, LocationBarBubbleDelegateView)

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_BUBBLE_DELEGATE_VIEW_H_

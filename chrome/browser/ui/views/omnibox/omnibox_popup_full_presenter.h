// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBar;
class OmniboxPopupPresenterDelegate;
class OmniboxController;

// Implements subclass of OmniboxPopupPresenterBase to present a single full
// WebUI (input row + suggestions dropdown) into the Omnibox popup.
class OmniboxPopupFullPresenter : public OmniboxPopupPresenterBase,
                                  public views::WidgetObserver,
                                  public ui::EventObserver {
 public:
  OmniboxPopupFullPresenter(LocationBar* location_bar,
                            OmniboxPopupPresenterDelegate& presenter_delegate,
                            OmniboxController* controller);
  OmniboxPopupFullPresenter(const OmniboxPopupFullPresenter&) = delete;
  OmniboxPopupFullPresenter& operator=(const OmniboxPopupFullPresenter&) =
      delete;
  ~OmniboxPopupFullPresenter() override;

  // OmniboxPopupPresenterBase:
  void Show() override;
  void Hide() override;
  void NotifyEscapeKeyPressed() override;
  // Requests activation of the popup widget and focuses the WebUI content,
  // while clearing stored focus on the container widget to prevent stealing
  // focus back from the WebUI input field.
  void RequestFocus() override;

  std::string_view GetPopupMetricPrefix() const override;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override;
  bool ShouldDebounceResize() const override;
  bool ShouldApplyHeightWorkarounds() const override;
  bool ShouldDetachWebContentsOnHide() const override;
  bool ShouldEvictOnHide() const override;
  bool ShouldSizeWebViewToPreferredHeight() const override;
  bool ShouldHideForInitialLayout() const override;

  bool IsDeactivating() const override;

 protected:
  // OmniboxPopupPresenterBase:
  // Returns true so that explicit focus requests (`focus_requested_`) are
  // preserved across asynchronous widget show/hide layout transitions.
  bool ShouldPreserveRequestedFocus() const override;
  std::unique_ptr<RoundedOmniboxResultsFrame> CreateResultsFrame(
      std::unique_ptr<views::View> contents,
      LocationBar* location_bar,
      bool forward_mouse_events) override;
  void SynchronizePopupBounds() override;
  void WidgetDestroyed() override;

 private:
  // views::WidgetObserver:
  // Handles window-wide focus shifts (e.g. clicking the webpage to activate
  // the parent window or switching apps). We must use this to detect that
  // focus has left the popup, since the `EventMonitor` is not notified if the
  // `FocusManager` restores focus to the same native omnibox view upon window
  // reactivation.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void StopForwardingEvents();

  // ui::EventObserver:
  // Handles click events and determines if the popup should be deactivated.
  void OnEvent(const ui::Event& event) override;

  void DeactivatePopupAndKillFocus();

  // Flag set when an ESC key event is intercepted before widget deactivation.
  bool is_handling_escape_key_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      popup_widget_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      parent_widget_observation_{this};

  // Used to determine where a click event happened to decide if the popup
  // should be deactivated.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Timer to stop forwarding events after a short delay.
  base::OneShotTimer forward_events_timer_;

  // Whether the "first shown" metrics have been logged at least once.
  bool logged_first_shown_metric_ = false;
  bool is_deactivating_ = false;

  base::WeakPtrFactory<OmniboxPopupFullPresenter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

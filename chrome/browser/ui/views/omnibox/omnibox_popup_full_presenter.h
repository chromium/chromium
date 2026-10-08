// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class AppMenuControl;
class LocationBar;
class OmniboxPopupPresenterDelegate;
class OmniboxController;
class OmniboxFullPopupWebUIContent;

// Implements subclass of OmniboxPopupPresenterBase to present a single full
// WebUI (input row + suggestions dropdown) into the Omnibox popup.
class OmniboxPopupFullPresenter : public OmniboxPopupPresenterBase,
                                  public views::WidgetObserver,
                                  public views::FocusChangeListener,
                                  public ui::EventObserver,
                                  public AppMenuButtonObserver {
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
  bool ShouldDrawShadowInWebUI() const override;
  bool ShouldHideForInitialLayout() const override;

  bool IsDeactivating() const override;
  bool ShouldReceiveFocus() const override;

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

  // Transparent margin around the popup content reserved for the drop shadow:
  // the full shadow insets when the dropdown is expanded, or the same insets
  // with `bottom = 0` when collapsed.
  gfx::Insets GetShadowMargin() const;

  OmniboxFullPopupWebUIContent* GetWebUIContent();

 private:
  // Where keyboard focus goes when `DeactivatePopupAndKillFocus()` blurs the
  // omnibox.
  enum class FocusAfterBlur {
    // The web contents, e.g. after the user clicked into the page.
    kWebContents,
    // Nowhere. The user moved to another window, so only Views focus is
    // cleared, as focusing the web contents would reactivate this window.
    kNone,
    // Wherever it already is. Another view in the browser window took focus,
    // so it's left there.
    kCurrent,
  };

  // views::WidgetObserver:
  // Observes the browser widget's activation (to keep the location bar as its
  // stored focus view while the popup is active) and the popup widget's own
  // activation (Escape handling, macOS key window restoration). The browser or
  // popup widget deactivating schedules `BlurIfBrowserWindowInactive()`.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void StopForwardingEvents();

  // views::FocusChangeListener:
  // Blurs the omnibox when another view in the browser window takes focus
  // while the omnibox is focused, e.g. the find bar on Ctrl/Cmd+F. The Views
  // omnibox is the browser window's focused view, so its `FocusManager` blurs
  // it when another view takes focus. The WebUI omnibox is focused in the
  // popup widget instead, so nothing else would blur it.
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // AppMenuButtonObserver:
  void AppMenuClosed() override;

  // ui::EventObserver:
  // Handles click events and determines if the popup should be deactivated.
  void OnEvent(const ui::Event& event) override;

  // Focuses the native Views content, underlying WebContents, and DOM input.
  void FocusPopupContent();
  // Blurs the omnibox and closes the popup unless it holds a draft. See
  // `FocusAfterBlur` for what happens to the browser window's focus.
  void DeactivatePopupAndKillFocus(FocusAfterBlur focus_after_blur);
  // Blurs the omnibox if the user left the browser window, i.e. neither the
  // browser widget nor its child widgets are active.
  void BlurIfBrowserWindowInactive();
  // If the omnibox is focused with the full popup open, blurs it after the
  // user moved to another window, and sets what the browser window restores
  // focus to when it's reactivated.
  void BlurForWindowDeactivation();

  // Returns the browser window's widget. In macOS immersive fullscreen this is
  // not the location bar's widget, which is then a non-activatable overlay.
  views::Widget* GetBrowserWidget();

  // Returns the browser window's focus manager, or null if there is none.
  views::FocusManager* GetBrowserFocusManager();

  // Flag set when an ESC key event is intercepted before widget deactivation.
  bool is_handling_escape_key_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      popup_widget_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_{this};
  // Observes the browser window's focus while the popup is shown. See
  // `OnDidChangeFocus()`.
  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      browser_focus_manager_observation_{this};
  base::ScopedObservation<AppMenuControl, AppMenuButtonObserver>
      app_menu_control_observation_{this};

  // Subscription to the browser widget's paint-as-active changes while the
  // popup is shown.
  base::CallbackListSubscription browser_paint_as_active_subscription_;

  // Used to determine where a click event happened to decide if the popup
  // should be deactivated.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Timer to stop forwarding events after a short delay.
  base::OneShotTimer forward_events_timer_;

  // Pending asynchronous focus and activation task.
  base::CancelableOnceClosure pending_focus_task_;

  // Whether the "first shown" metrics have been logged at least once.
  bool logged_first_shown_metric_ = false;
  bool is_deactivating_ = false;

  base::WeakPtrFactory<OmniboxPopupFullPresenter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/range/range.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class AppMenuControl;
class LocationBar;
class OmniboxPopupPresenterDelegate;
class OmniboxController;
class OmniboxPopupUI;
template <typename T>
class WebUIContentsWrapperT;

namespace ui {
class TrackedElement;
}  // namespace ui

// Implements subclass of OmniboxPopupPresenterBase to present a single full
// WebUI (input row + suggestions dropdown) into the Omnibox popup.
class OmniboxPopupFullPresenter : public OmniboxPopupPresenterBase,
                                  public views::WidgetObserver,
                                  public views::FocusChangeListener,
                                  public ui::EventObserver,
                                  public AppMenuButtonObserver,
                                  public views::ViewObserver {
 public:
  OmniboxPopupFullPresenter(LocationBar* location_bar,
                            OmniboxPopupPresenterDelegate& presenter_delegate,
                            OmniboxController* controller);
  OmniboxPopupFullPresenter(const OmniboxPopupFullPresenter&) = delete;
  OmniboxPopupFullPresenter& operator=(const OmniboxPopupFullPresenter&) =
      delete;
  ~OmniboxPopupFullPresenter() override;

  // Where keyboard focus goes when `DeactivatePopupAndKillFocus()` blurs the
  // omnibox.
  enum class FocusAfterBlur {
    // The web contents, e.g. after the user clicked into the page.
    kWebContents,
    // Nowhere. Another window, such as a bubble, took activation, so only
    // Views focus is cleared, as focusing the web contents would reactivate
    // this window.
    kNone,
    // Wherever it already is. Another view in the browser window took focus,
    // so it's left there.
    kCurrent,
  };

  // Blurs the omnibox and closes the popup unless it holds a draft, i.e. user
  // text or keyword mode, through `OmniboxPopupViewFullWebUI::BlurOmnibox()`.
  // Adds the widget's own steps: drops pending focus work and the permission
  // prompt state, and moves the browser window's focus as `FocusAfterBlur`
  // says.
  void DeactivatePopupAndKillFocus(FocusAfterBlur focus_after_blur);

  // OmniboxPopupPresenterBase:
  void Show() override;
  void Hide() override;
  void OnOmniboxFocused() override;
  void NotifyEscapeKeyPressed() override;
  // Requests activation of the popup widget and focuses the WebUI content,
  // while clearing stored focus on the container widget to prevent stealing
  // focus back from the WebUI input field.
  void RequestFocus() override;
  std::string_view GetPopupMetricPrefix() const override;
  // Calls `DeactivatePopupAndKillFocus(FocusAfterBlur::kNone)`.
  void DeactivateAndKillFocus() override;

  bool ShouldReceiveFocus() const override;

  // Called when a native file dialog opened from the popup's "+" menu is shown
  // and closed. See `OnModalDialogShown()`. Selecting files opens AI Mode with
  // them, so the omnibox isn't refocused then. Unlike deactivation blockers,
  // these are called regardless of `omnibox::kOmniboxKeepOpenOnFileSelection`.
  // TODO(b/582886941): Merge deactivation blockers with these.
  //
  // Other modal dialogs are detected through
  // `OnWidgetWindowModalVisibilityChanged()`. That can't tell whether a file
  // was selected, though. Also, it reports native file dialogs on macOS, which
  // are window-modal sheets, but not on other platforms (crbug.com/450705434).
  // TODO(b/581984657): Handle other native file dialogs on those platforms,
  // such as the one Ctrl+O opens.
  void OnFileDialogOpened() override;
  void OnFileDialogClosed(bool file_selected) override;

  using FocusStateToRestore = OmniboxPopupHost::FocusStateToRestore;
  std::optional<FocusStateToRestore> TakeFocusStateToRestoreAfterModalDialog()
      override;
  void OnTabChanged() override;

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
  void OnWidgetPresentedChanged(bool presented) override;
  std::unique_ptr<OmniboxPopupWebUIBaseContent> CreateWebUIContent(
      std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents) override;
  void OnWebUIContentsAdopted() override;
  void OnWebUIContentsReleased() override;

  // Transparent margin around the popup content reserved for the drop shadow:
  // the full shadow insets when the dropdown is expanded, or the same insets
  // with `bottom = 0` when collapsed.
  gfx::Insets GetShadowMargin() const;

  OmniboxFullPopupWebUIContent* GetWebUIContent() override;

 private:
  // views::WidgetObserver:
  // Observes the browser widget's activation (to keep the location bar as its
  // stored focus view while the popup is active) and the popup widget's own
  // activation (Escape handling, macOS key window restoration). The browser or
  // popup widget deactivating schedules `BlurIfBrowserOrPopupInactive()`.
  // Either activating while `refocus_on_activation_` is set schedules
  // `RefocusAfterActivation()`.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  // Keeps the popup over the location bar when the browser widget moves or
  // resizes. The popup is its own top-level window, which doesn't always move
  // with the browser window, e.g. a macOS child window stays put when its
  // parent is resized from the top edge.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  // Stops observing the browser widget, which may still be observed while the
  // popup is hidden. See `Hide()`.
  void OnWidgetDestroying(views::Widget* widget) override;
  // Reports window-modal dialogs to `OnModalDialogShown()` and
  // `OnModalDialogClosed()`.
  void OnWidgetWindowModalVisibilityChanged(views::Widget* widget,
                                            bool visible) override;
  void StopForwardingEvents();

  // Tells this window's popup view that the adopted page's handler is ready,
  // if the page bound it in its previous window. Otherwise
  // `OmniboxPopupUI::CreatePageHandler()` tells it once the page binds it.
  void NotifyPopupHandlerReadyIfBound();

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

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view,
                               bool visible) override;
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // ui::EventObserver:
  // Handles click and touch events and determines if the popup should be
  // deactivated.
  void OnEvent(const ui::Event& event) override;

  // Focuses the native Views content, underlying WebContents, and DOM input.
  void FocusPopupContent();
  // Blurs the omnibox unless the browser window, including the popup, is
  // active. Any other window taking activation, including a bubble anchored to
  // the browser window, blurs it, as with the Views omnibox. On ChromeOS, the
  // system emoji picker is the exception, until it closes.
  void BlurIfBrowserOrPopupInactive();
  // If the omnibox is focused with the full popup open, blurs it after another
  // window, such as a bubble, took activation, and sets what the browser window
  // restores focus to when it's reactivated. If a modal dialog took
  // activation, also closes the popup, even if a draft would keep it open, and
  // even if the omnibox was already blurred. See `OnModalDialogShown()`.
  void BlurForWindowDeactivation();
  // Hands focus to the popup if the omnibox is still focused with the popup
  // open. Runs after the browser window or popup is activated while
  // `refocus_on_activation_` is set.
  void RefocusAfterActivation();

#if BUILDFLAG(IS_CHROMEOS)
  // Returns whether the system emoji picker is shown, e.g. after picking Emoji
  // in the omnibox's context menu.
  static bool IsEmojiPickerShown();
  // Checks again whether to blur the omnibox once the emoji picker closes.
  void OnEmojiPickerHidden(ui::TrackedElement* element);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Returns whether the popup shows only its input row, with nothing painted
  // below the location bar.
  bool IsPopupCollapsed() const;

  // Clips the part of the collapsed popup that the expanded vertical tab strip
  // or the organizer tray covers, which also sends events there to the view
  // beneath.
  void UpdatePopupOcclusion();

  // Called when a modal dialog, e.g. a native file dialog, is shown over the
  // browser window or the popup, and when it closes. While it's shown, the
  // omnibox is blurred, like for any other window taking activation, and the
  // popup is closed even if it shows a draft, so that it can't be used until
  // the dialog closes, as with the classic omnibox. Once the dialog closes and
  // the browser window is reactivated, the omnibox is refocused with the text
  // (including any draft) and selection it had when the dialog was shown. If
  // the omnibox wasn't focused, but a draft kept the popup open, the popup is
  // reopened unfocused instead.
  void OnModalDialogShown();
  void OnModalDialogClosed();

  // Posts `BlurIfBrowserOrPopupInactive()` after `OnModalDialogShown()`.
  void BlurIfBrowserOrPopupInactiveSoon();

  // Refocuses the omnibox after a modal dialog closed, if the dialog blurred it
  // and the browser window is active again. See `OnModalDialogShown()`.
  void RestoreFocusAfterModalDialog();

  // Clears `focus_state_before_modal_dialog_`. If the popup is hidden, also
  // stops observing the browser window, which `Hide()` keeps observing while
  // that state is set.
  void ClearFocusStateBeforeModalDialog();

  // Returns the text shown in the omnibox: the user's text while editing, and
  // otherwise the full URL if `show_full_url`, or the permanent display text.
  std::u16string GetOmniboxText(bool show_full_url);

  // Returns the browser window's widget. In macOS immersive fullscreen this is
  // not the location bar's widget, which is then a non-activatable overlay.
  views::Widget* GetBrowserWidget();

  // Returns whether the browser widget or this popup's widget is active. This
  // is true while the omnibox is focused, since the popup then holds
  // activation, and false once another window, such as a bubble anchored to
  // the browser window, takes activation. Returns false if there is no browser
  // widget.
  bool IsBrowserOrPopupActive();

  // Returns whether a focus request may activate the popup. This is
  // `IsBrowserOrPopupActive()`, plus, on Mac, a child widget such as a bubble
  // anchored to the browser window holding activation.
  bool CanTakeFocus();

  // Returns the browser window's focus manager, or null if there is none.
  views::FocusManager* GetBrowserFocusManager();

  // Activation moves between the browser window and the popup in two steps,
  // deactivating one before activating the other. Keeps the browser window
  // painting as active in between, until a posted task.
  void KeepBrowserPaintingAsActiveAcrossHandoff();
  void ReleaseBrowserPaintAsActiveLockIfPopupInactive();

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
  // Observes the vertical tab strip and the organizer tray while the popup is
  // shown. See `UpdatePopupOcclusion()`.
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      occluding_view_observations_{this};
#if BUILDFLAG(IS_CHROMEOS)
  // Observed while shown: the immersive reveal moves the top container
  // without resizing the location bar.
  base::ScopedObservation<views::View, views::ViewObserver>
      top_container_observation_{this};
#endif

  // Used to determine where a click event happened to decide if the popup
  // should be deactivated.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Timer to stop forwarding events after a short delay.
  base::OneShotTimer forward_events_timer_;

  // Pending asynchronous focus and activation task.
  base::CancelableOnceClosure pending_focus_task_;

  // Set when `RequestFocus()` couldn't activate the popup because another
  // window held activation, leaving the omnibox focused with the popup open
  // but inactive. Cleared once the browser window or popup is activated again,
  // which refocuses the popup, or when the omnibox is blurred or the popup is
  // hidden.
  bool refocus_on_activation_ = false;

#if BUILDFLAG(IS_CHROMEOS)
  // Set while the omnibox is kept focused for the emoji picker. See
  // `BlurIfBrowserOrPopupInactive()`.
  base::CallbackListSubscription emoji_picker_hidden_subscription_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Keeps the browser window painting as active while the popup is active, and
  // until a posted task after the browser window or popup deactivates.
  std::unique_ptr<views::Widget::PaintAsActiveLock>
      browser_paint_as_active_lock_;

  // Whether the "first shown" metrics have been logged at least once.
  bool logged_first_shown_metric_ = false;

  // Set when a modal dialog is shown while the popup is open. If the omnibox
  // was focused, holds its text and selection, and whether it showed the full
  // URL. Cleared once the omnibox is refocused, or the popup reopened, after
  // the dialog closes. Also cleared if the omnibox is still focused when the
  // dialog closes. That happens if the dialog never took activation. For
  // example, when policy blocks a file dialog, it reports that it closed
  // without ever being shown.
  struct FocusStateBeforeModalDialog {
    // False if the popup was open with a draft while the omnibox wasn't
    // focused. The popup is then reopened, unfocused, once the dialog closes.
    bool had_focus = false;
    std::u16string text;
    gfx::Range selection;
    bool show_full_url = false;
  };
  std::optional<FocusStateBeforeModalDialog> focus_state_before_modal_dialog_;
  // Whether a native file dialog opened from the popup's "+" menu is open. See
  // `OnFileDialogOpened()`.
  bool file_dialog_open_ = false;

  base::WeakPtrFactory<OmniboxPopupFullPresenter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

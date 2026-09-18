// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBar;
class OmniboxPopupWebUIBaseContent;
class OmniboxPopupPresenterDelegate;
class RoundedOmniboxResultsFrame;
class OmniboxController;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
struct PresentationFeedback;
}  // namespace gfx

namespace omnibox {
extern const void* kOmniboxWebUIPopupWidgetId;
}  // namespace omnibox

// An interface representing a deactivation blocker. Keeping an instance of this
// interface alive prevents the omnibox popup widget from closing when it loses
// focus.
class OmniboxPopupDeactivationBlocker {
 public:
  virtual ~OmniboxPopupDeactivationBlocker() = default;
};

// A base assistant class for OmniboxPopupViewWebUI, this manages "n" WebViews
// and a Widget to present the WebUI. This class is an implementation detail and
// is not expected to grow or change much with omnibox changes.  The concern of
// this class is presentation only, i.e. Views and Widgets.  For omnibox logic
// concerns and communication between native omnibox code and the WebUI code,
// work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenterBase
    : public content::WebContentsObserver,
      public SearchboxHandler::Delegate,
      public permissions::PermissionRequestManager::Observer {
 public:
  // An RAII-style helper that registers itself as a blocker upon creation and
  // unregisters itself when destroyed.
  class ScopedDeactivationBlocker : public OmniboxPopupDeactivationBlocker {
   public:
    explicit ScopedDeactivationBlocker(
        base::WeakPtr<OmniboxPopupPresenterBase> presenter);
    ScopedDeactivationBlocker(const ScopedDeactivationBlocker&) = delete;
    ScopedDeactivationBlocker& operator=(const ScopedDeactivationBlocker&) =
        delete;
    ~ScopedDeactivationBlocker() override;

   private:
    base::WeakPtr<OmniboxPopupPresenterBase> presenter_;
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRoundedResultsFrame);
  // Arguments must outlast this.
  explicit OmniboxPopupPresenterBase(
      LocationBar* location_bar,
      OmniboxPopupPresenterDelegate& presenter_delegate,
      OmniboxController* controller);
  OmniboxPopupPresenterBase(const OmniboxPopupPresenterBase&) = delete;
  OmniboxPopupPresenterBase& operator=(const OmniboxPopupPresenterBase&) =
      delete;
  ~OmniboxPopupPresenterBase() override;

  // Creates and returns a new deactivation blocker. The caller is responsible
  // for managing the lifecycle of the returned blocker (typically via
  // std::unique_ptr). Returns nullptr if
  // `omnibox::kOmniboxKeepOpenOnFileSelection` is disabled.
  virtual std::unique_ptr<OmniboxPopupDeactivationBlocker>
  CreateDeactivationBlocker();
  virtual void OnFileSelectionClosed();
  virtual void NotifyEscapeKeyPressed() {}
  bool has_active_blockers() const { return deactivation_blockers_count_ > 0; }

  // Show or hide the popup widget with web view.
  virtual void Show();
  virtual void Hide();

  // Tells whether the popup widget exists.
  bool IsShown() const;

  // Request focus on the popup widget and its web contents.
  virtual void RequestFocus();

  // Caches the height of the WebUI content, which is then used to compute the
  // popup widget bounds.
  void OnContentHeightChanged(int content_height);

  // Synchronize the popup widget's bounds to its anchor (location bar view).
  virtual void SynchronizePopupBounds();

  // Returns the currently "active" Popup content, whichever one is visible or
  // going to be visible within the popup.
  OmniboxPopupWebUIBaseContent* GetWebUIContent();
  const OmniboxPopupWebUIBaseContent* GetWebUIContent() const;

  // Returns the timeout if showing should be deferred until the WebUI has
  // painted a new frame, or std::nullopt if it should not be deferred.
  virtual std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const = 0;

  // Returns whether resize events should be debounced.
  virtual bool ShouldDebounceResize() const = 0;

  // Returns whether height workarounds should be applied.
  virtual bool ShouldApplyHeightWorkarounds() const = 0;
  // Returns if the WebContents should be detached when the popup is hidden.
  virtual bool ShouldDetachWebContentsOnHide() const = 0;

  // Returns whether the popup should evict its saved compositor frame when
  // hidden.
  virtual bool ShouldEvictOnHide() const = 0;

  // Returns if the child WebView bounds should be sized to its preferred
  // height in RoundedOmniboxResultsFrame.
  virtual bool ShouldSizeWebViewToPreferredHeight() const = 0;

  // Returns whether the popup's drop shadow is painted by the page rather than
  // by the results frame. When true the frame paints nothing and the WebView
  // is expanded to cover the shadow margin.
  virtual bool ShouldDrawShadowInWebUI() const;

  virtual std::string_view GetPopupMetricPrefix() const = 0;

  OmniboxPopupPresenterDelegate& delegate() const {
    return *presenter_delegate_;
  }
  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // SearchboxHandler::Delegate:
  void OnEmbeddedPermissionDialogChanged(bool is_showing,
                                         const gfx::Size& prompt_size) override;
  OmniboxController* GetOmniboxController() override;

  views::Widget* get_widget_for_testing() { return widget_.get(); }

  views::Widget* GetWidget() const { return widget_.get(); }

  void set_widget_for_testing(std::unique_ptr<views::Widget> widget) {
    widget_ = std::move(widget);
  }

  const gfx::Size& get_minimum_size() const { return minimum_size_; }

  // Outermost view in the hierarchy; used for hit testing.
  views::View* GetOuterView();

  // Sets explicit permission prompt showing state. Called by permission request
  // observer callbacks, PermissionPromptFactory, and WebUI media request
  // handlers.
  void SetPermissionPromptShowing(bool showing);

  // Resets prompt showing and dismissal state flags.
  void ResetPermissionPromptShowingState();

  // Handles common dismissal state updates when a permission prompt is closed.
  void HandlePermissionPromptDismissal();

  // Returns true if a permission prompt is showing or being dismissed,
  // which should prevent out-of-focus activation events from hiding the popup.
  bool IsPermissionPromptPreventingClose() const;

  // Returns true if the presenter is currently deactivating.
  virtual bool IsDeactivating() const;

  // Returns whether the WebUI content view receives focus.
  virtual bool ShouldReceiveFocus() const;

 protected:
  inline static constexpr std::string_view kWebUIPopupMetricPrefix =
      "Omnibox.Popup.WebUI";
  inline static constexpr std::string_view kAimPopupMetricPrefix =
      "Omnibox.Popup.Aim";
  inline static constexpr std::string_view kFullWebUIPopupMetricPrefix =
      "Omnibox.Popup.FullWebUI";

  // The container for the WebUI WebView.
  views::View* GetUIContainer() const;

  // Sets the webview content reference.
  void SetWebUIContent(
      std::unique_ptr<OmniboxPopupWebUIBaseContent> webui_content);

  void EnsureWidgetCreated();

  // Whether the widget should be torn down when the popup is hidden and rebuilt
  // when it is shown again, rather than simply hidden and re-shown. Guards
  // against the reshown native window presenting compositor content left over
  // from the previous show.
  bool ShouldDestroyWidgetOnHide() const;

  // True only while `Hide()` is deliberately destroying the widget so that the
  // next `Show()` rebuilds it. `WidgetDestroyed()` overrides must consult this
  // before treating the destruction as external (e.g. by the OS) and tearing
  // down popup state, which is already being torn down.
  bool is_destroying_widget() const { return is_destroying_widget_; }

  // Called when the widget has just been destroyed.
  virtual void WidgetDestroyed() {}

  // Hook to create the results frame view.
  virtual std::unique_ptr<RoundedOmniboxResultsFrame> CreateResultsFrame(
      std::unique_ptr<views::View> contents,
      LocationBar* location_bar,
      bool forward_mouse_events);

  // Returns the frame view of the widget, or null if there is no widget. The
  // widget is absent while hidden when `ShouldDestroyWidgetOnHide()` is on.
  RoundedOmniboxResultsFrame* GetResultsFrame() const;

  // Returns whether or not the popup should include the location bar cutout.
  virtual bool ShouldShowLocationBarCutout() const;

  // Returns true if the popup widget should start transparent to allow the
  // initial layout pass to complete without visual artifacts.
  virtual bool ShouldHideForInitialLayout() const;

  // Returns true if explicit focus requests should be preserved across widget
  // show/hide rather than unconditionally re-requesting focus after show.
  virtual bool ShouldPreserveRequestedFocus() const;

  // Logs the ResultToContentReady metric. This is called synchronously when the
  // visual state is ready.
  virtual void LogResultToContentReadyMetric(base::TimeTicks result_ready_time,
                                             bool is_first_show);

  LocationBar* location_bar() const { return location_bar_.get(); }

  OmniboxController* controller() const { return controller_.get(); }

  // permissions::PermissionRequestManager::Observer:
  void OnPromptAdded() override;
  void OnPromptRemoved() override;
  void OnPromptRecreateViewFailed() override;
  void OnPromptCreationFailedHiddenTab() override;
  void OnRequestsFinalized() override;
  void OnPermissionRequestManagerDestructed() override;

  // Called by subclasses when widget activation changes to active = true.
  void OnWidgetActivated();

  // Whether focus has been explicitly requested since the popup was shown.
  bool focus_requested_ = false;

  // The height of the popup content. Can be 0 if not specified.
  int content_height_ = 0;

 private:
  friend class OmniboxPopupViewWebUITest;
  friend class OmniboxWebUiInteractiveTest;
  friend class OmniboxPopupPresenterBaseTest;

  base::ScopedObservation<permissions::PermissionRequestManager,
                          permissions::PermissionRequestManager::Observer>
      permission_observation_{this};

  // Set to true when a permission prompt is added/showing.
  bool is_prompt_showing_ = false;

  // Set to true when a permission prompt is removed to prevent the omnibox
  // popup from closing due to activation loss while focus is being restored.
  bool is_handling_prompt_dismissal_ = false;

  void OnWidgetClosed(views::Widget::ClosedReason closed_reason);

  // Shows the popup widget immediately, called after stale frame fix deferral
  // if enabled.
  void ShowWidget(base::TimeTicks show_widget_time);

  // Callback for when the visual state is ready.
  void OnVisualStateReady(base::TimeTicks show_widget_time,
                          base::TimeTicks result_ready_time,
                          bool from_fallback,
                          bool success);

  // Closes the widget, extracting the WebUI container first so it survives.
  // `widget_` is always cleared synchronously, but destroying the
  // `views::Widget` object is always deferred, since a close can arrive from
  // inside a `views::Widget` callback that still touches it after we return.
  // The destructor drains the deferred widgets inline, so teardown there
  // remains synchronous.
  void ReleaseWidget();

  // Destroys the widgets handed off by `ReleaseWidget()`.
  void DeletePendingWidgets();

  // The location bar that owns `this`.
  const raw_ptr<LocationBar> location_bar_;

  const raw_ref<OmniboxPopupPresenterDelegate> presenter_delegate_;

  // The container for both the WebUI suggestions list and other WebUI
  // containers
  std::unique_ptr<views::View> owned_omnibox_popup_webui_container_;

  // The WebUI content WebView. Owned by the container.
  raw_ptr<OmniboxPopupWebUIBaseContent> omnibox_popup_webui_content_ = nullptr;

  // The popup widget that contains this WebView. Created, closed and owned by
  // `this`; see `ReleaseWidget()` for how it is destroyed.
  std::unique_ptr<views::Widget> widget_;

  // Widgets closed by `ReleaseWidget()` whose destruction was deferred to
  // unwind the stack first. A queue rather than a single slot so that
  // back-to-back closes within one task cannot destroy an earlier widget
  // synchronously from inside a later one's teardown. Owned so the widgets are
  // still destroyed if the presenter goes away before the posted deletion runs.
  std::vector<std::unique_ptr<views::Widget>> widgets_pending_deletion_;

  const raw_ptr<OmniboxController> controller_;

  // True if `ShowWidget()` execution is currently being deferred until the
  // WebUI has produced a new frame.
  bool is_deferred_ = false;

  // True only for the duration of the deliberate widget teardown performed by
  // `Hide()` when `ShouldDestroyWidgetOnHide()` holds. See
  // `is_destroying_widget()`.
  bool is_destroying_widget_ = false;

  // Whether the first content ready metric of the popup has been logged.
  bool has_logged_first_content_ready_ = false;

  // Whether the first paint metric of the popup has been logged.
  bool has_logged_first_widget_paint_ = false;

  // Minimum size bounds of omnibox popup.
  gfx::Size minimum_size_;

  friend class ScopedDeactivationBlocker;

  void RegisterBlocker();
  void UnregisterBlocker();

  void OnWidgetPresented(base::TimeTicks show_request_time,
                         const gfx::PresentationFeedback& feedback);

  // The number of active deactivation blockers currently registered. If this is
  // greater than zero, out-of-focus widget deactivations will be ignored.
  int deactivation_blockers_count_ = 0;

  // Weak pointer factory for callbacks related to visual state.
  base::WeakPtrFactory<OmniboxPopupPresenterBase> visual_state_weak_factory_{
      this};
  // Weak pointer factory for general callbacks.
  base::WeakPtrFactory<OmniboxPopupPresenterBase> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_

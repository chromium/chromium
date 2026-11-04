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
#include "ui/views/widget/widget_observer.h"

class LocationBar;
class OmniboxPopupUI;
class OmniboxPopupWebUIBaseContent;
class OmniboxPopupPresenterDelegate;
class OmniboxPopupWebUIPoolClient;
class RoundedOmniboxResultsFrame;
class OmniboxController;
enum class OmniboxPopupState;
template <typename T>
class WebUIContentsWrapperT;

namespace base {
struct Feature;
}  // namespace base

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
  // Why `GetOrCreateWebUIContent()` was called. Recorded in traces.
  enum class CreationReason {
    kEager,
    kAfterWindowPresentation,
    kShow,
    kContextMenu,
    kOmniboxFocus,
  };

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
  std::unique_ptr<OmniboxPopupDeactivationBlocker> CreateDeactivationBlocker();
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

  // Returns the WebUI content, first creating it with `CreateWebUIContent()`
  // if it does not exist yet. Creating the content creates a WebContents and
  // starts its navigation, so call this only where the content is needed,
  // e.g. to show the popup. Use `GetWebUIContent()` to read the content
  // without creating it. If the popup is shared across windows, this adopts
  // an idle popup from another browser window of the profile instead, and
  // skips prewarming (`kEager`, `kAfterWindowPresentation`) when another
  // window already has a popup of this kind; it then returns null.
  // Otherwise returns null only for unit-test presenters without WebUI
  // content.
  OmniboxPopupWebUIBaseContent* GetOrCreateWebUIContent(CreationReason reason);

  // Releases the loaded WebContents of the popup, which must be hidden, so
  // that the presenter of the same popup kind in another browser window of the
  // profile can adopt it with `AdoptWebUIContents()`. Destroys this
  // presenter's content; the next `GetOrCreateWebUIContent()` creates a new
  // one.
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> ReleaseWebUIContents();

  // Returns whether `ReleaseWebUIContents()` may move the popup to another
  // browser window now. Errs toward false: the popup must have loaded content,
  // be hidden, hold no deactivation blocker or permission prompt, and its
  // content must hold no work for this window; see
  // `OmniboxPopupWebUIBaseContent::CanReleaseContentsWrapper()`.
  bool CanReleaseWebUIContents();

  // Returns the kind of popup that this presenter shares with the other
  // browser windows of its profile with
  // `omnibox::kOmniboxWebUIPopupShareAcrossWindows`, or
  // `OmniboxPopupState::kNone` if it does not share its popup.
  virtual OmniboxPopupState GetSharedPopupKind() const;

  // Creates this presenter's content around `contents`, which another browser
  // window's presenter released, instead of loading a new WebContents. The
  // presenter must not have content.
  OmniboxPopupWebUIBaseContent* AdoptWebUIContents(
      std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents);

  // Called when the browser window that hosts this popup presents a frame
  // after `CreateWebUIContentAfterWindowPresentation()`. If the WebUI content
  // does not exist yet, posts a task that creates it. Later calls do nothing.
  // With `kOnOmniboxFocus`, it posts the task only if the omnibox has already
  // been focused, and otherwise leaves creation to `OnOmniboxFocused()`.
  void OnBrowserWindowPresented();

  // Called when the omnibox of the window that hosts this popup gains focus.
  // When the presenter's creation strategy is `kOnOmniboxFocus`, creates the
  // WebUI content once the browser window has presented its first frame, so
  // the page loads while the user types without delaying window presentation.
  void OnOmniboxFocused();

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

  // Returns whether the popup's page has completed its first visually
  // non-empty paint. `LocationBarView` drops its native focus ring under the
  // full popup once this is true. The full popup's content reports the paint
  // through the delegate's `OnPopupFirstVisuallyNonEmptyPaint()`.
  bool CompletedFirstVisuallyNonEmptyPaint();
  void SetCompletedFirstVisuallyNonEmptyPaintForTesting(bool completed);

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

  // Returns whether the WebUI content view receives focus.
  virtual bool ShouldReceiveFocus() const;

 protected:
  // When the WebUI content is created ahead of time. With every strategy,
  // `GetOrCreateWebUIContent()` creates the content immediately if it is
  // needed sooner.
  enum class WebUIContentCreationStrategy {
    // In the constructor.
    kEager,
    // After the browser window presents its first frame.
    kAfterWindowPresentation,
    // Once the omnibox has been focused and the browser window has presented
    // its first frame.
    kOnOmniboxFocus,
    // Never ahead of time.
    kOnDemand,
  };

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

  // Presenters that create their WebUI content lazily override this and call
  // `ApplyWebUIContentCreationStrategy()` from their constructor. The content
  // adopts `contents` if set, and loads a new WebContents otherwise. Returns
  // null by default.
  virtual std::unique_ptr<OmniboxPopupWebUIBaseContent> CreateWebUIContent(
      std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents);

  // Returns the strategy for a presenter whose deferred creation is controlled
  // by `defer_creation_feature`, taking
  // `omnibox::ShouldCreatePopupWebUIOnDemand()` into account.
  static WebUIContentCreationStrategy GetWebUIContentCreationStrategy(
      const base::Feature& defer_creation_feature);

  // Returns the strategy to use when the presenter's deferred creation feature
  // is enabled. Presenters whose feature params defer creation further
  // override this.
  virtual WebUIContentCreationStrategy GetDeferredWebUIContentCreationStrategy()
      const;

  // Applies the strategy for `defer_creation_feature`: the one from
  // `GetWebUIContentCreationStrategy()`, with
  // `GetDeferredWebUIContentCreationStrategy()` in place of
  // `kAfterWindowPresentation`. Called from the constructor of presenters that
  // override `CreateWebUIContent()`.
  void ApplyWebUIContentCreationStrategy(
      const base::Feature& defer_creation_feature);

  // Creates the WebUI content now, requests its creation after the browser
  // window's first presentation, or does nothing, according to `strategy`.
  void ApplyWebUIContentCreationStrategy(WebUIContentCreationStrategy strategy);

  void EnsureWidgetCreated();

  // Whether the widget should be torn down when the popup is hidden and rebuilt
  // when it is shown again, rather than simply hidden and re-shown. Guards
  // against the reshown native window presenting compositor content left over
  // from the previous show. The base implementation applies
  // `kOmniboxFullWebUIDestroyWidgetOnHide` to every popup when the full
  // WebUI omnibox is enabled; subclasses may additionally enable it via a
  // per-popup flag.
  virtual bool ShouldDestroyWidgetOnHide() const;

  // True only while `Hide()` is deliberately destroying the widget so that the
  // next `Show()` rebuilds it. `WidgetDestroyed()` overrides must consult this
  // before treating the destruction as external (e.g. by the OS) and tearing
  // down popup state, which is already being torn down.
  bool is_destroying_widget() const { return is_destroying_widget_; }

  // Called when the widget has just been destroyed.
  virtual void WidgetDestroyed() {}

  // Called with false on show/hide, and true once the first frame after show
  // is presented.
  virtual void OnWidgetPresentedChanged(bool presented) {}

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

  // Waits until the browser window that hosts this popup presents a frame while
  // shown and unminimized, then calls `OnBrowserWindowPresented()`. Used by
  // `ApplyWebUIContentCreationStrategy()`.
  void CreateWebUIContentAfterWindowPresentation();

  // Observes the browser window's top-level widget for
  // `CreateWebUIContentAfterWindowPresentation()`.
  class BrowserWidgetObserver : public views::WidgetObserver {
   public:
    explicit BrowserWidgetObserver(OmniboxPopupPresenterBase& presenter);
    ~BrowserWidgetObserver() override;

    // views::WidgetObserver:
    void OnWidgetDestroying(views::Widget* widget) override;
    void OnWidgetVisibilityChanged(views::Widget* widget,
                                   bool visible) override;
    void OnWidgetShowStateChanged(views::Widget* widget) override;

   private:
    const raw_ref<OmniboxPopupPresenterBase> presenter_;
  };

  // Requests a presentation callback while the browser widget is shown and
  // unminimized, and cancels it otherwise.
  void UpdateBrowserWidgetPresentationRequest();
  void OnBrowserWidgetPresented();
  void StopObservingBrowserWidget();

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

  WebUIContentCreationStrategy content_creation_strategy_ =
      WebUIContentCreationStrategy::kOnDemand;

  // Whether the browser window that hosts this popup has presented a frame.
  bool has_browser_window_presented_ = false;

  // Whether the omnibox of the window that hosts this popup has gained focus.
  bool omnibox_focused_ = false;

  // Whether `OnBrowserWindowPresented()` has posted the creation task.
  bool deferred_creation_scheduled_ = false;

  BrowserWidgetObserver browser_widget_observer_{*this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_{&browser_widget_observer_};

  // Set if this presenter shares its popup with the profile's other windows.
  std::unique_ptr<OmniboxPopupWebUIPoolClient> pool_client_;

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

  // Tracks whether the omnibox considers the popup open. Used by `Hide()` to
  // detect if external window management (e.g. Ash on a user switch) hid the
  // widget without the presenter's knowledge. In which case, the widget must be
  // discarded rather than cached so it is not re-shown later.
  bool is_open_ = false;

  // True only for the duration of the deliberate widget teardown performed by
  // `Hide()` when `ShouldDestroyWidgetOnHide()` holds or the widget was hidden
  // externally. See `is_destroying_widget()`.
  bool is_destroying_widget_ = false;

  // Whether the first content ready metric of the popup has been logged.
  bool has_logged_first_content_ready_ = false;

  // Whether the first paint metric of the popup has been logged.
  bool has_logged_first_widget_paint_ = false;

  // Overrides `CompletedFirstVisuallyNonEmptyPaint()` when set.
  std::optional<bool> completed_first_visually_non_empty_paint_for_testing_;

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
  // Vends the outstanding browser window presentation callback, if any.
  // Invalidated to cancel it.
  base::WeakPtrFactory<OmniboxPopupPresenterBase>
      browser_presentation_weak_factory_{this};
  // Weak pointer factory for general callbacks.
  base::WeakPtrFactory<OmniboxPopupPresenterBase> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_

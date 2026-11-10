// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_context_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_observer.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/sequence_bound.h"
#include "base/win/windows_types.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_shortcut_win.h"
#endif

class Profile;
class ScopedKeepAlive;
class SkBitmap;

namespace views {
class UnhandledKeyboardEventHandler;
}  // namespace views

namespace omnibox_everywhere {

#if defined(USE_AURA)
class OmniboxEverywhereEventHandlerAura;
#endif
class OmniboxEverywhereRegionSelectOverlay;
class OmniboxEverywhereWidgetDelegate;

// Manages the desktop Omnibox Everywhere native window (views::Widget)
// lifecycle and handles switching between different profiles.
class OmniboxEverywhereUIManager
    : public views::WidgetObserver,
      public WebUIContentsWrapper::Host,
      public BrowserCollectionObserver,
      public OmniboxEverywhereContextMenuController::Delegate,
      public PermissionPromptObserver::Observer {
 public:
  // Modal surfaces owned by the widget that keep it from being dismissed on
  // deactivation.
  enum class Modal {
    kFileChooser,
    kDrivePicker,
    kHotkeyDropdown,
    kScreensharePicker,
    kScreenshareDisclosure,
    kPermissionPrompt,
    kMinValue = kFileChooser,
    kMaxValue = kPermissionPrompt,
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOmniboxEverywhereElementId);

  // Fixed popup window width:
  //   680px (Loomnibox searchbox content width; 480px when smallLoomnibox param
  //          is enabled)
  // +  48px (24px left + 24px right body padding in omnibox_everywhere.html to
  //          accommodate the drop shadow without clipping).
  // = 728px total window width (528px when smallLoomnibox is enabled).
  static constexpr int kPopupFixedWidth = 728;
  static constexpr int kPopupSmallFixedWidth = 528;
  // Dynamic sizing popup window widths (32px left + 32px right body padding):
  // - Expanded: 680px (Large) or 600px (Medium) content width + 64px = 744px /
  //             664px.
  // - Resting (persistent mode): 480px (Small) content width + 64px = 544px.
  static constexpr int kDynamicPopupLargeFixedWidth = 744;
  static constexpr int kDynamicPopupMediumFixedWidth = 664;
  static constexpr int kDynamicPopupSmallFixedWidth = 544;
  static int GetPopupFixedWidth();
  static int GetPopupExpandedFixedWidth();
  static constexpr int kDefaultRestingHeight = 152;
  // Resting height when the FRE intro modal is shown (searchbox + 2-row FRE
  // modal card + drop-shadow body padding).
  static constexpr int kFreModalRestingHeight = 424;
  static constexpr base::TimeDelta kActivationGracePeriod =
      base::Milliseconds(500);
  static constexpr base::TimeDelta kPostHideCaptureDuration =
      base::Milliseconds(200);
  // How often to check whether the mouse button has been released while a
  // dismissal is deferred (see `HandleWidgetDeactivated()`).
  static constexpr base::TimeDelta kPendingDismissalPollInterval =
      base::Milliseconds(50);

  using ContentsWrapperFactory =
      base::RepeatingCallback<std::unique_ptr<WebUIContentsWrapper>(Profile*)>;

  explicit OmniboxEverywhereUIManager(
      ContentsWrapperFactory contents_wrapper_factory = {});
  OmniboxEverywhereUIManager(const OmniboxEverywhereUIManager&) = delete;
  OmniboxEverywhereUIManager& operator=(const OmniboxEverywhereUIManager&) =
      delete;
  ~OmniboxEverywhereUIManager() override;

  void ShowForProfile(Profile* profile,
                      gfx::NativeWindow context = gfx::NativeWindow());

  // Closes the Omnibox Everywhere widget.
  void Close();

  // Demotes the widget to normal Z-order and deactivates it without hiding.
  void Demote();

#if BUILDFLAG(IS_WIN)
  // Minimizes the widget to the taskbar. Unlike `Close()`, the widget and its
  // WebContents are kept alive, and no FRE impression is recorded.
  void Minimize();
#endif  // BUILDFLAG(IS_WIN)

  // Synchronously closes the widget and destroys the WebContents during profile
  // shutdown.
  void Shutdown();

  // Returns true if the widget is visible and not minimized.
  bool IsVisible() const;

  // Returns true if the widget is active/focused.
  bool IsActive() const;

  // Returns true if any modal dialog or modal overlay owned by the launcher is
  // open (e.g. file chooser, drive picker, permission prompt, screenshare
  // picker/disclosure, region select overlay, or hotkey dropdown).
  bool HasOpenModalDialog() const;

  // Returns true while a screenshot capture flow owns the screen: the native
  // OS screen picker, Chrome's default desktop media picker, the region
  // selection overlay, or the capture that immediately follows the user's
  // selection. The widget is intentionally hidden for the duration of that
  // flow, so invocation entry points must be ignored while this is true.
  bool IsScreenshareCaptureInProgress() const;

  // Shows the overflow (three-dot) menu containing the Omnibox Everywhere
  // settings items, anchored below `anchor_screen_rect` (screen DIPs). Shares
  // the context menu controller with the right-click menu. `on_closed` runs
  // when the menu closes, or immediately if it cannot be shown.
  void ShowOverflowMenu(const gfx::Rect& anchor_screen_rect,
                        ui::mojom::MenuSourceType source_type,
                        base::OnceClosure on_closed);

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetVisibilityOnScreenChanged(views::Widget* widget,
                                         bool visible) override;
  void OnWidgetShowStateChanged(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetUserDragStarted(views::Widget* widget) override;
  void OnWidgetUserDragEnded(views::Widget* widget) override;

  // WebUIContentsWrapper::Host:
  void CloseUI() override;
  void ShowUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // PermissionPromptObserver::Observer:
  void OnPermissionPromptChanged(bool is_showing,
                                 const gfx::Size& prompt_size) override;

  void OnFileChooserOpened();
  void OnFileChooserClosed();

  void OnDrivePickerOpened();
  void OnDrivePickerClosed();

  void OnHotkeyDropdownOpened();
  void OnHotkeyDropdownClosed();

  void OnDragEntered();
  void OnDragExited();

  using RegionCaptureSource = OmniboxEverywhereService::RegionCaptureSource;

  void OnScreensharePickerOpened();
  void OnScreensharePickerClosed();
  bool CancelChromeDefaultPicker(Profile* target_profile = nullptr);

  void ShowScreenshotDisclosureDialog(
      base::OnceClosure on_accepted,
      base::OnceClosure on_cancelled = base::DoNothing());
  using RegionSelectedCallback =
      base::OnceCallback<void(const SkBitmap& result_bitmap)>;
  void ShowRegionSelectOverlay(const SkBitmap& screenshot,
                               const RegionCaptureSource& source,
                               RegionSelectedCallback callback);
  void OnRegionSelectOverlayClosed(RegionSelectedCallback callback,
                                   const SkBitmap& result_bitmap);

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {}
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override {}

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }
  views::Widget* widget() { return widget_.get(); }
  const views::Widget* widget() const { return widget_.get(); }
  content::WebContents* web_contents() const;
  OmniboxEverywhereWidgetDelegate* widget_delegate();
  const OmniboxEverywhereWidgetDelegate* widget_delegate() const;

  bool IsPointInDraggableRegion(const gfx::Point& point) const;

#if BUILDFLAG(IS_WIN)
  // Creates the Start Menu shortcut the Shell requires for taskbar pinning,
  // on a COM STA runner. Reports whether a usable shortcut exists.
  void CreateStartMenuShortcut(
      base::OnceCallback<void(bool)> callback = base::DoNothing());

  // Suppresses taskbar pinning on widgets created from now on; pinning remains
  // enabled until this is called. Does not affect the open widget.
  void DisableTaskbarPinning();

  bool taskbar_pinning_disabled_for_testing() const {
    return taskbar_pinning_disabled_;
  }
  bool start_menu_shortcut_requested_for_testing() const {
    return start_menu_shortcut_requested_;
  }
  // Returns whether a window is on the current virtual desktop.
  using IsOnCurrentVirtualDesktopCallback = base::RepeatingCallback<bool(HWND)>;
  void SetIsOnCurrentVirtualDesktopCallbackForTesting(
      IsOnCurrentVirtualDesktopCallback callback) {
    is_on_current_virtual_desktop_ = std::move(callback);
  }
#endif

  // For testing:
  WebUIContentsWrapper* contents_wrapper_for_testing() {
    return contents_wrapper_.get();
  }
  const std::optional<SkRegion>& draggable_region_for_testing() const {
    return draggable_region_;
  }
  bool IsModalOpenForTesting(Modal modal) const {
    return open_modals_.Has(modal);
  }
  views::Widget* disclosure_dialog_widget_for_testing() {
    return disclosure_dialog_widget_.get();
  }
  bool is_hotkey_dropdown_deactivation_task_pending_for_testing() const {
    return !hotkey_dropdown_deactivation_task_.IsCancelled();
  }
  void CheckDeactivationAfterHotkeyDropdownClosedForTesting() {
    CheckDeactivationAfterHotkeyDropdownClosed();
  }
  OmniboxEverywhereRegionSelectOverlay* region_select_overlay_for_testing() {
    return region_select_overlay_.get();
  }
  OmniboxEverywhereContextMenuController*
  context_menu_controller_for_testing() {
    return context_menu_controller_.get();
  }
  bool is_dragging_for_testing() const { return is_dragging_; }
#if defined(USE_AURA)
  OmniboxEverywhereEventHandlerAura* event_handler_for_testing() {
    return event_handler_.get();
  }
#endif
  bool is_demoted() const { return is_demoted_; }

 private:
  using ModalSet = base::EnumSet<Modal, Modal::kMinValue, Modal::kMaxValue>;

  void EnsureContentsWrapperInitialized(Profile* profile);
  // Creates the widget. If `bounds` is not set, uses the default position.
  void CreateAndInitWidget(gfx::NativeWindow context,
                           std::optional<gfx::Rect> bounds);
  void ActivateAndFocus();
  void OnEphemeralModelPrefChanged();

#if BUILDFLAG(IS_WIN)
  // Disables taskbar pinning if no Start Menu shortcut is available.
  void OnStartMenuShortcutChecked(bool shortcut_exists);
#endif
  void OnMostVisitedPrefChanged();
  OmniboxEverywhereService* GetOmniboxEverywhereService();
  // Ends the active FRE session when the popup is closed or destroyed. Note
  // that `Demote()` (persistent mode deactivation) and window minimization
  // intentionally preserve the active FRE session across reactivations.
  void EndFreSession();
  void MaybeShowLensPromo();
  void EndLensPromo();
  static gfx::Rect CalculateWidgetBounds(int height);
  void ApplyPendingAutoResize();
  void AdjustWidgetBoundsToWorkArea();

  // Try and acquire process and profile keep alives. If unsuccessful, releases
  // keep alives (if any) and returns false.
  bool TryAcquireKeepAlives();
  // Try and acquire process and profile keep alives. Returns true if
  // successful, false otherwise.
  bool AcquireKeepAlives();
  void ReleaseKeepAlives();

  std::unique_ptr<WebUIContentsWrapper> CreateContentsWrapper(Profile* profile);

  // OmniboxEverywhereContextMenuController::Delegate:
  views::Widget* GetContextMenuHostWidget() override;
  content::WebContents* GetContextMenuWebContents() override;
  Profile* GetContextMenuProfile() override;
  void OnContextMenuClosed() override;
#if BUILDFLAG(IS_WIN)
  void MinimizeFromContextMenu() override;
  void CloseFromContextMenu() override;
#endif  // BUILDFLAG(IS_WIN)

  // Destroys the widget and the WebContents, and releases keep-alives.
  void DestroyWidgetAndWebContents();

  // Destroys the widget and the UI it owns (dialogs, menus, overlays) and
  // resets transient UI state, but keeps the WebContents so the next widget
  // reuses it. Unlike DestroyWidgetAndWebContents(), safe to call from within a
  // call on the WebContents.
  void DestroyWidget();

  // Cancels any pending deactivation tasks or deferred-dismissal poll timer.
  void CancelPendingDeactivation();

  // Cancels transient UI state (pending dismissal tasks, activation grace
  // period timestamp, and any open context menu).
  void CancelTransientUiState();
  void OnWidgetClosed(views::Widget::ClosedReason reason);
  void HandleWidgetDeactivated();
  // Runs on `pending_dismissal_timer_` until the mouse button is released,
  // then either re-focuses the widget (if a drag entered the WebContents and
  // was dropped onto it) or performs the deferred dismissal.
  void OnPendingDismissalPoll();
  void CheckDeactivationAfterHotkeyDropdownClosed();
  void OnScreenshotDisclosureClosed(base::OnceClosure on_accepted,
                                    base::OnceClosure on_cancelled,
                                    views::Widget::ClosedReason reason);
  void SetModalOpen(Modal modal, bool open);
  void UpdateModalInteractionState();

#if defined(USE_AURA)
  std::unique_ptr<OmniboxEverywhereEventHandlerAura> event_handler_;
#endif

  // The native window hosting the Omnibox Everywhere UI.
  raw_ptr<Profile> profile_ = nullptr;
  ContentsWrapperFactory contents_wrapper_factory_;

  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  std::unique_ptr<OmniboxEverywhereWidgetDelegate> widget_delegate_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<OmniboxEverywhereRegionSelectOverlay> region_select_overlay_;

  std::unique_ptr<views::Widget> disclosure_dialog_widget_;
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
  base::OneShotTimer capture_release_timer_;

  // Tracks the modal surfaces (file chooser, pickers, permission prompt, etc.)
  // that keep the widget from being dismissed. The region select overlay is
  // tracked by `region_select_overlay_` instead.
  ModalSet open_modals_;
  bool is_demoted_ = false;
  bool suppress_restore_on_screenshare_picker_closed_ = false;
  bool is_dragging_ = false;
  // True while an external drag is hovering over `web_contents()` during a
  // deferred dismissal (`PreHandleDragUpdate()` fired without a subsequent
  // `PreHandleDragExit()`).
  bool is_drag_over_web_contents_ = false;
  // Re-entrancy guard to prevent recursive closing or processing deactivation
  // events while Close() is executing synchronously on the stack.
  bool is_closing_ = false;
  // Re-entrancy guard to prevent OnWidgetBoundsChanged from re-entering when
  // AdjustWidgetBoundsToWorkArea() or ResizeDueToAutoResize() updates bounds.
  bool is_adjusting_bounds_ = false;
  std::optional<gfx::Size> pending_auto_resize_size_;
  std::optional<SkRegion> draggable_region_;

  std::unique_ptr<views::UnhandledKeyboardEventHandler>
      unhandled_keyboard_event_handler_;
  std::unique_ptr<OmniboxEverywhereContextMenuController>
      context_menu_controller_;

  std::optional<base::TimeTicks> last_shown_time_;
  // Task posted when the widget is deactivated, used to either dismiss or
  // reactivate the widget after the grace period.
  base::CancelableOnceClosure deactivation_task_;
  base::CancelableOnceClosure hotkey_dropdown_deactivation_task_;
  // Running while a deactivation-triggered dismissal is deferred because a
  // mouse button was held at deactivation time (potential drag onto the
  // widget). See `HandleWidgetDeactivated()`.
  base::RepeatingTimer pending_dismissal_timer_;

  PrefChangeRegistrar local_state_pref_change_registrar_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedObservation<PermissionPromptObserver,
                          PermissionPromptObserver::Observer>
      permission_prompt_observation_{this};

#if BUILDFLAG(IS_WIN)
  base::SequenceBound<OmniboxEverywhereShortcutHelperWin> shortcut_helper_;

  IsOnCurrentVirtualDesktopCallback is_on_current_virtual_desktop_;

  // Tracks whether shortcut creation has succeeded or is in flight.
  // Reset on failure so subsequent persistent widgets can retry.
  bool start_menu_shortcut_requested_ = false;

  // See DisableTaskbarPinning().
  bool taskbar_pinning_disabled_ = false;
#endif

  base::WeakPtrFactory<OmniboxEverywhereUIManager> weak_factory_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_

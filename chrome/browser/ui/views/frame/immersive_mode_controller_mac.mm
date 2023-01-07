// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AppKit/AppKit.h>

#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/cocoa/scoped_menu_bar_lock.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils_mac.h"

namespace {
class ImmersiveModeControllerMac : public ImmersiveModeController,
                                   public views::FocusChangeListener,
                                   public views::ViewObserver,
                                   public views::WidgetObserver,
                                   public web_app::AppRegistrarObserver {
 public:
  class RevealedLock : public ImmersiveRevealedLock {
   public:
    explicit RevealedLock(base::WeakPtr<ImmersiveModeControllerMac> controller);

    RevealedLock(const RevealedLock&) = delete;
    RevealedLock& operator=(const RevealedLock&) = delete;

    ~RevealedLock() override;

   private:
    base::WeakPtr<ImmersiveModeControllerMac> controller_;
  };

  ImmersiveModeControllerMac();

  ImmersiveModeControllerMac(const ImmersiveModeControllerMac&) = delete;
  ImmersiveModeControllerMac& operator=(const ImmersiveModeControllerMac&) =
      delete;

  ~ImmersiveModeControllerMac() override;

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  std::unique_ptr<ImmersiveRevealedLock> GetRevealedLock(
      AnimateReveal animate_reveal) override;
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override;
  bool ShouldStayImmersiveAfterExitingFullscreen() override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // Immersive fullscreen has started.
  void FullScreenOverlayViewWillAppear();

  // views::FocusChangeListener implementation.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::ViewObserver implementation
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver implementation
  void OnWidgetDestroying(views::Widget* widget) override;

  // web_app::AppRegistrarObserver
  void OnAlwaysShowToolbarInFullscreenChanged(const web_app::AppId& app_id,
                                              bool show) override;

 private:
  friend class RevealedLock;

  // void Layout(AnimateReveal);
  void LockDestroyed();
  void SetMenuRevealed(bool revealed);

  // Handler of show_fullscreen_toolbar_ changes.
  void UpdateToolbarVisibility();

  raw_ptr<BrowserView> browser_view_ = nullptr;  // weak
  std::unique_ptr<ImmersiveRevealedLock> focus_lock_;
  std::unique_ptr<ImmersiveRevealedLock> menu_lock_;
  bool enabled_ = false;
  int revealed_lock_count_ = 0;
  base::ScopedObservation<views::View, views::ViewObserver>
      top_container_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_frame_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      overlay_widget_observation_{this};

  // Used to keep track of the update of kShowFullscreenToolbar preference.
  BooleanPrefMember show_fullscreen_toolbar_;
  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::AppRegistrarObserver>
      always_show_toolbar_in_fullscreen_observation_{this};

  // Used as a convenience to access
  // NativeWidgetMacNSWindowHost::GetNSWindowMojo().
  raw_ptr<remote_cocoa::mojom::NativeWidgetNSWindow> ns_window_mojo_ =
      nullptr;  // weak

  base::WeakPtrFactory<ImmersiveModeControllerMac> weak_ptr_factory_;
};

}  // namespace

ImmersiveModeControllerMac::RevealedLock::RevealedLock(
    base::WeakPtr<ImmersiveModeControllerMac> controller)
    : controller_(std::move(controller)) {}

ImmersiveModeControllerMac::RevealedLock::~RevealedLock() {
  if (auto* controller = controller_.get())
    controller->LockDestroyed();
}

ImmersiveModeControllerMac::ImmersiveModeControllerMac()
    : weak_ptr_factory_(this) {}

ImmersiveModeControllerMac::~ImmersiveModeControllerMac() {
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ImmersiveModeControllerMac::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
  ns_window_mojo_ = views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
                        browser_view_->GetWidget()->GetNativeWindow())
                        ->GetNSWindowMojo();

  if (web_app::AppBrowserController::IsWebApp(browser_view->browser())) {
    auto* provider =
        web_app::WebAppProvider::GetForWebApps(browser_view->GetProfile());
    always_show_toolbar_in_fullscreen_observation_.Observe(
        &provider->registrar());
  } else {
    show_fullscreen_toolbar_.Init(
        prefs::kShowFullscreenToolbar, browser_view->GetProfile()->GetPrefs(),
        base::BindRepeating(
            &ImmersiveModeControllerMac::UpdateToolbarVisibility,
            base::Unretained(this)));
  }
}

void ImmersiveModeControllerMac::UpdateToolbarVisibility() {
  bool always_show_toolbar;
  if (web_app::AppBrowserController::IsWebApp(browser_view_->browser())) {
    web_app::AppBrowserController* controller =
        browser_view_->browser()->app_controller();
    always_show_toolbar = controller->AlwaysShowToolbarInFullscreen();
  } else {
    always_show_toolbar = *show_fullscreen_toolbar_;
  }
  ns_window_mojo_->UpdateToolbarVisibility(always_show_toolbar);

  // TODO(bur): Re-layout so that "no show" -> "always show" will work
  // properly.
}

void ImmersiveModeControllerMac::SetMenuRevealed(bool revealed) {
  if (revealed) {
    if (!menu_lock_)
      menu_lock_ = GetRevealedLock(ANIMATE_REVEAL_NO);
  } else {
    if (menu_lock_)
      menu_lock_.reset();
  }
  browser_view_->InvalidateLayout();
}

void ImmersiveModeControllerMac::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    browser_view_->GetWidget()->GetFocusManager()->AddFocusChangeListener(this);
    top_container_observation_.Observe(browser_view_->top_container());
    browser_frame_observation_.Observe(browser_view_->GetWidget());
    overlay_widget_observation_.Observe(browser_view_->overlay_widget());

    // Capture the overlay content view before enablement. Once enabled the view
    // is moved to an AppKit window leaving us otherwise without a reference.
    NSView* content_view = browser_view_->overlay_widget()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()
                               .contentView;
    browser_view_->overlay_widget()->SetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kImmersiveContentNSView,
        content_view);

    views::NativeWidgetMacNSWindowHost* overlay_host =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
            browser_view_->overlay_widget()->GetNativeWindow());
    ns_window_mojo_->EnableImmersiveFullscreen(
        overlay_host->bridged_native_widget_id(),
        base::BindOnce(
            &ImmersiveModeControllerMac::FullScreenOverlayViewWillAppear,
            base::Unretained(this)));

    // TODO(bur): Figure out why this Show() is needed.
    // Overlay content view will not be displayed unless we call Show() on the
    // overlay_widget. This is odd since the view has been reparented to a
    // different NSWindow.
    browser_view_->overlay_widget()->Show();

    // If the window is maximized OnViewBoundsChanged will not be called
    // when transitioning to full screen. Call it now.
    OnViewBoundsChanged(browser_view_->top_container());
  } else {
    browser_view_->GetWidget()->GetFocusManager()->RemoveFocusChangeListener(
        this);
    top_container_observation_.Reset();
    browser_frame_observation_.Reset();
    overlay_widget_observation_.Reset();

    // Notify BrowserView about the fullscreen exit so that the top container
    // can be reparented, otherwise it might be destroyed along with the
    // overlay widget.
    for (Observer& observer : observers_)
      observer.OnImmersiveFullscreenExited();

    // Rollback the view shuffling from enablement.
    browser_view_->overlay_widget()->Hide();
    ns_window_mojo_->DisableImmersiveFullscreen();
    browser_view_->overlay_widget()->SetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kImmersiveContentNSView, nullptr);

    menu_lock_.reset();
    focus_lock_.reset();
  }
}

bool ImmersiveModeControllerMac::IsEnabled() const {
  return enabled_;
}

bool ImmersiveModeControllerMac::ShouldHideTopViews() const {
  return enabled_ && !IsRevealed();
}

bool ImmersiveModeControllerMac::IsRevealed() const {
  return enabled_;
}

int ImmersiveModeControllerMac::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  return 0;
}

std::unique_ptr<ImmersiveRevealedLock>
ImmersiveModeControllerMac::GetRevealedLock(AnimateReveal animate_reveal) {
  revealed_lock_count_++;
  if (enabled_ && revealed_lock_count_ == 1)
    browser_view_->OnImmersiveRevealStarted();
  return std::make_unique<RevealedLock>(weak_ptr_factory_.GetWeakPtr());
}

void ImmersiveModeControllerMac::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {}

bool ImmersiveModeControllerMac::ShouldStayImmersiveAfterExitingFullscreen() {
  return false;
}

void ImmersiveModeControllerMac::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {}

void ImmersiveModeControllerMac::FullScreenOverlayViewWillAppear() {
  SetMenuRevealed(true);
}

void ImmersiveModeControllerMac::OnWillChangeFocus(views::View* focused_before,
                                                   views::View* focused_now) {}

void ImmersiveModeControllerMac::OnDidChangeFocus(views::View* focused_before,
                                                  views::View* focused_now) {
  if (browser_view_->top_container()->Contains(focused_now)) {
    if (!focus_lock_)
      focus_lock_ = GetRevealedLock(ANIMATE_REVEAL_NO);
  } else {
    focus_lock_.reset();
  }
}

void ImmersiveModeControllerMac::OnViewBoundsChanged(
    views::View* observed_view) {
  if (!observed_view->bounds().IsEmpty()) {
    browser_view_->overlay_widget()->SetBounds(observed_view->bounds());
    ns_window_mojo_->OnTopContainerViewBoundsChanged(observed_view->bounds());
    UpdateToolbarVisibility();
  }
}

void ImmersiveModeControllerMac::OnWidgetDestroying(views::Widget* widget) {
  SetEnabled(false);
}

void ImmersiveModeControllerMac::OnAlwaysShowToolbarInFullscreenChanged(
    const web_app::AppId& app_id,
    bool show) {
  if (web_app::AppBrowserController::IsForWebApp(browser_view_->browser(),
                                                 app_id)) {
    UpdateToolbarVisibility();
  }
}

void ImmersiveModeControllerMac::LockDestroyed() {
  revealed_lock_count_--;
  if (revealed_lock_count_ == 0)
    browser_view_->OnImmersiveRevealEnded();
}

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeControllerMac() {
  return std::make_unique<ImmersiveModeControllerMac>();
}

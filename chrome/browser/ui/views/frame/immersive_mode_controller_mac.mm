// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/cocoa/immersive_mode_delegate_mac.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils_mac.h"

// A stub NSWindowDelegate class that will be used to map the AppKit controlled
// NSWindow to the overlay view widget's NSWindow. The delegate will be used to
// help with input routing.
@interface ImmersiveModeMapper : NSObject <ImmersiveModeDelegate>
@property(assign) NSWindow* originalHostingWindow;
@end

@implementation ImmersiveModeMapper
@synthesize originalHostingWindow = _originalHostingWindow;
@end

// Host of the overlay view.
@interface ImmersiveModeTitlebarViewController
    : NSTitlebarAccessoryViewController {
  base::mac::ScopedBlock<void (^)()> _view_will_appear_handler;
  base::mac::ScopedBlock<void (^)()> _view_did_appear_handler;
}
@end

@implementation ImmersiveModeTitlebarViewController

- (instancetype)initWithHandlers:(void (^)())viewWillAppearHandle
             viewDidAppearHandle:(void (^)())viewDidAppearHandle {
  if ((self = [super init])) {
    _view_will_appear_handler.reset([viewWillAppearHandle copy]);
    _view_did_appear_handler.reset([viewDidAppearHandle copy]);
  }
  return self;
}

- (void)viewWillAppear {
  [super viewWillAppear];
  _view_will_appear_handler.get()();

  // TODO(bur): Get the updated width from OnViewBoundsChanged
  NSView* tab_view = self.view;
  NSRect f = tab_view.frame;
  f.size.width = 2400;
  tab_view.frame = f;
  for (NSView* view in tab_view.subviews) {
    if ([view isKindOfClass:[BridgedContentView class]]) {
      view.frame = tab_view.frame;
    }
  }
}

- (void)viewDidAppear {
  [super viewDidAppear];
  _view_did_appear_handler.get()();
}

@end

// An NSView that will set the ImmersiveModeDelegate on the AppKit created
// window that ends up hosting this view via the
// NSTitlebarAccessoryViewController API.
@interface ImmersiveModeView : NSView
- (instancetype)initWithImmersiveModeDelegate:
    (id<ImmersiveModeDelegate>)delegate;
@end

@implementation ImmersiveModeView {
  ImmersiveModeMapper* _fullscreenDelegate;
}

- (instancetype)initWithImmersiveModeDelegate:
    (id<ImmersiveModeDelegate>)delegate {
  self = [super init];
  if (self) {
    _fullscreenDelegate = delegate;
  }
  return self;
}

- (void)viewWillMoveToWindow:(NSWindow*)window {
  if (views::IsNSToolbarFullScreenWindow(window)) {
    // This window is created by AppKit. Make sure it doesn't have a delegate so
    // we can use it for out own purposes.
    DCHECK(!window.delegate);
    window.delegate = _fullscreenDelegate;
  }
}

@end

namespace {
class ImmersiveModeControllerMac : public ImmersiveModeController,
                                   public views::FocusChangeListener,
                                   public views::ViewObserver,
                                   public views::WidgetObserver {
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

  // views::FocusChangeListener implementation.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::ViewObserver implementation
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver implementation
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class RevealedLock;

  // void Layout(AnimateReveal);
  void LockDestroyed();
  void SetMenuRevealed(bool revealed);

  // Handler of show_fullscreen_toolbar_ changes.
  void ShowFullscreenToolbar();

  raw_ptr<BrowserView> browser_view_ = nullptr;  // weak
  std::unique_ptr<ImmersiveRevealedLock> focus_lock_;
  std::unique_ptr<ImmersiveRevealedLock> menu_lock_;
  bool enabled_ = false;
  int revealed_lock_count_ = 0;
  base::scoped_nsobject<ImmersiveModeTitlebarViewController>
      immersive_mode_titlebar_view_controller_;
  base::scoped_nsobject<ImmersiveModeMapper> immersive_mode_mapper_;
  base::ScopedObservation<views::View, views::ViewObserver>
      top_container_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_frame_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      overlay_widget_observation_{this};

  // Used to keep track of the update of kShowFullscreenToolbar preference.
  BooleanPrefMember show_fullscreen_toolbar_;

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
  show_fullscreen_toolbar_.Init(
      prefs::kShowFullscreenToolbar, browser_view->GetProfile()->GetPrefs(),
      base::BindRepeating(&ImmersiveModeControllerMac::ShowFullscreenToolbar,
                          base::Unretained(this)));
}

void ImmersiveModeControllerMac::ShowFullscreenToolbar() {
  if (*show_fullscreen_toolbar_) {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight =
        immersive_mode_titlebar_view_controller_.get().view.frame.size.height;
    browser_view_->GetWidget()
        ->GetNativeWindow()
        .GetNativeNSWindow()
        .styleMask &= ~NSWindowStyleMaskFullSizeContentView;
  } else {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight = 0;
    browser_view_->GetWidget()
        ->GetNativeWindow()
        .GetNativeNSWindow()
        .styleMask |= NSWindowStyleMaskFullSizeContentView;
  }

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

    // Create a new NSTitlebarAccessoryViewController that will host the
    // overlay_view_.
    NSView* contentView = browser_view_->overlay_widget()
                              ->GetNativeWindow()
                              .GetNativeNSWindow()
                              .contentView;
    immersive_mode_titlebar_view_controller_.reset(
        [[ImmersiveModeTitlebarViewController alloc]
            initWithHandlers:^() {
              SetMenuRevealed(true);
            }
            viewDidAppearHandle:^() {
              browser_view_->overlay_widget()->SetNativeWindowProperty(
                  views::NativeWidgetMacNSWindowHost::kImmersiveContentNSView,
                  contentView);
            }]);

    // Create a NSWindow delegate that will be used to map the AppKit created
    // NSWindow to the overlay view widget's NSWindow.
    immersive_mode_mapper_.reset([[ImmersiveModeMapper alloc] init]);
    immersive_mode_mapper_.get().originalHostingWindow =
        browser_view_->overlay_widget()->GetNativeWindow().GetNativeNSWindow();
    immersive_mode_titlebar_view_controller_.get().view =
        [[ImmersiveModeView alloc]
            initWithImmersiveModeDelegate:immersive_mode_mapper_.get()];

    // Remove the content view from the overlay view widget's NSWindow. This
    // view will be re-parented into the AppKit created NSWindow.
    NSView* overlay_content_view = browser_view_->overlay_widget()
                                       ->GetNativeWindow()
                                       .GetNativeNSWindow()
                                       .contentView;
    [overlay_content_view removeFromSuperview];

    // Add the overlay view to the accessory view controller and hand everything
    // over to AppKit.
    [immersive_mode_titlebar_view_controller_.get().view
        addSubview:overlay_content_view];
    immersive_mode_titlebar_view_controller_.get().layoutAttribute =
        NSLayoutAttributeBottom;
    [browser_view_->GetWidget()->GetNativeWindow().GetNativeNSWindow()
        addTitlebarAccessoryViewController:
            immersive_mode_titlebar_view_controller_];

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
    NSView* overlay_content_view =
        immersive_mode_titlebar_view_controller_.get()
            .view.subviews.firstObject;
    [overlay_content_view removeFromSuperview];
    browser_view_->overlay_widget()
        ->GetNativeWindow()
        .GetNativeNSWindow()
        .contentView = overlay_content_view;
    [immersive_mode_titlebar_view_controller_ removeFromParentViewController];
    [immersive_mode_titlebar_view_controller_.get().view release];
    immersive_mode_titlebar_view_controller_.reset();
    browser_view_->GetWidget()
        ->GetNativeWindow()
        .GetNativeNSWindow()
        .styleMask |= NSWindowStyleMaskFullSizeContentView;

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
  return enabled_ && revealed_lock_count_ > 0;
}

int ImmersiveModeControllerMac::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  return (enabled_ && !IsRevealed()) ? -top_container_size.height() : 0;
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
  browser_view_->overlay_widget()->SetBounds(observed_view->bounds());
  NSRect frame_rect = observed_view->bounds().ToCGRect();
  immersive_mode_titlebar_view_controller_.get().view.frame = frame_rect;
  ShowFullscreenToolbar();
}

void ImmersiveModeControllerMac::OnWidgetDestroying(views::Widget* widget) {
  SetEnabled(false);
}

void ImmersiveModeControllerMac::LockDestroyed() {
  revealed_lock_count_--;
  if (revealed_lock_count_ == 0)
    browser_view_->OnImmersiveRevealEnded();
}

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeControllerMac() {
  return std::make_unique<ImmersiveModeControllerMac>();
}

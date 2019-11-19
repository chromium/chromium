// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AppKit/AppKit.h>

#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/cocoa/scoped_menu_bar_lock.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#import "ui/base/cocoa/tracking_area.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_observer.h"

namespace {
const CGFloat kMenuBarLockPadding = 50;
}

// MenuRevealMonitor tracks visibility of the menu bar associated with |window|,
// and calls |handler| when it changes. In fullscreen, when the mouse pointer
// moves to or away from the top of the screen, |handler| will be called several
// times with a number between zero and one indicating how much of the menu bar
// is visible.
@interface MenuRevealMonitor : NSObject
- (instancetype)initWithWindow:(NSWindow*)window
                 changeHandler:(void (^)(double))handler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

@implementation MenuRevealMonitor {
  base::mac::ScopedBlock<void (^)(double)> change_handler_;
  base::scoped_nsobject<NSTitlebarAccessoryViewController> accVC_;
}

- (instancetype)initWithWindow:(NSWindow*)window
                 changeHandler:(void (^)(double))handler {
  if ((self = [super init])) {
    change_handler_.reset([handler copy]);
    accVC_.reset([[NSTitlebarAccessoryViewController alloc] init]);
    auto* accVC = accVC_.get();
    accVC.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
    [accVC addObserver:self
            forKeyPath:@"revealAmount"
               options:NSKeyValueObservingOptionNew
               context:nil];
    [window addTitlebarAccessoryViewController:accVC];
  }
  return self;
}

- (void)dealloc {
  [accVC_ removeObserver:self forKeyPath:@"revealAmount"];
  [accVC_ removeFromParentViewController];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  double revealAmount =
      base::mac::ObjCCastStrict<NSNumber>(change[NSKeyValueChangeNewKey])
          .doubleValue;
  change_handler_.get()(revealAmount);
}
@end

// ImmersiveToolbarOverlayView performs two functions. First, it hitTests to its
// superview (BridgedContentView) to block mouse events from hitting siblings
// which the toolbar might overlap, like RenderWidgetHostView. It also sets up a
// tracking area which locks the menu bar's visibility while the mouse pointer
// is within its bounds, plus some padding at the bottom.
@interface ImmersiveToolbarOverlayView : NSView
@property(nonatomic) BOOL menuBarLockingEnabled;
@end

@implementation ImmersiveToolbarOverlayView {
  ui::ScopedCrTrackingArea trackingArea_;
  std::unique_ptr<ScopedMenuBarLock> menuBarLock_;
}
@synthesize menuBarLockingEnabled = menuBarLockingEnabled_;

- (void)setMenuBarLockingEnabled:(BOOL)menuBarLockingEnabled {
  if (menuBarLockingEnabled == menuBarLockingEnabled_)
    return;
  menuBarLockingEnabled_ = menuBarLockingEnabled;
  [self updateTrackingArea];
}

- (void)updateTrackingArea {
  NSRect trackingRect = self.bounds;
  trackingRect.origin.y -= kMenuBarLockPadding;
  trackingRect.size.height += kMenuBarLockPadding;

  if (CrTrackingArea* trackingArea = trackingArea_.get()) {
    if (menuBarLockingEnabled_ && NSEqualRects(trackingRect, trackingArea.rect))
      return;
    else
      [self removeTrackingArea:trackingArea];
  }

  if (menuBarLockingEnabled_) {
    trackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:trackingRect
             options:NSTrackingMouseEnteredAndExited |
                     NSTrackingActiveInKeyWindow
               owner:self
            userInfo:nil]);
    [self addTrackingArea:trackingArea_.get()];
  } else {
    trackingArea_.reset();
    menuBarLock_.reset();
  }
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self updateTrackingArea];
}

- (NSView*)hitTest:(NSPoint)point {
  NSPoint pointInView = [self convertPoint:point fromView:self.superview];
  if (NSPointInRect(pointInView, self.visibleRect))
    return self.superview;
  return [super hitTest:point];
}

- (void)mouseEntered:(NSEvent*)event {
  menuBarLock_ = std::make_unique<ScopedMenuBarLock>();
}

- (void)mouseExited:(NSEvent*)event {
  menuBarLock_.reset();
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
    RevealedLock(base::WeakPtr<ImmersiveModeControllerMac> controller,
                 AnimateReveal animate_reveal);
    ~RevealedLock() override;

   private:
    base::WeakPtr<ImmersiveModeControllerMac> controller_;
    AnimateReveal animate_reveal_;
    DISALLOW_COPY_AND_ASSIGN(RevealedLock);
  };

  ImmersiveModeControllerMac();
  ~ImmersiveModeControllerMac() override;

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  ImmersiveRevealedLock* GetRevealedLock(AnimateReveal animate_reveal) override
      WARN_UNUSED_RESULT;
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
  void LockDestroyed(AnimateReveal);
  void SetMenuRevealed(bool revealed);

  BrowserView* browser_view_ = nullptr;  // weak
  std::unique_ptr<ImmersiveRevealedLock> focus_lock_;
  std::unique_ptr<ImmersiveRevealedLock> menu_lock_;
  bool enabled_ = false;
  int revealed_lock_count_ = 0;
  base::scoped_nsobject<ImmersiveToolbarOverlayView> overlay_view_;
  base::scoped_nsobject<NSObject> menu_reveal_monitor_;

  base::WeakPtrFactory<ImmersiveModeControllerMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerMac);
};

}  // namespace

ImmersiveModeControllerMac::RevealedLock::RevealedLock(
    base::WeakPtr<ImmersiveModeControllerMac> controller,
    AnimateReveal animate_reveal)
    : controller_(std::move(controller)), animate_reveal_(animate_reveal) {}

ImmersiveModeControllerMac::RevealedLock::~RevealedLock() {
  if (auto* controller = controller_.get())
    controller->LockDestroyed(animate_reveal_);
}

ImmersiveModeControllerMac::ImmersiveModeControllerMac()
    : weak_ptr_factory_(this) {}

ImmersiveModeControllerMac::~ImmersiveModeControllerMac() = default;

void ImmersiveModeControllerMac::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
}

void ImmersiveModeControllerMac::SetMenuRevealed(bool revealed) {
  if (revealed) {
    if (!menu_lock_)
      menu_lock_.reset(GetRevealedLock(ANIMATE_REVEAL_YES));
    overlay_view_.get().menuBarLockingEnabled = YES;
  } else {
    if (menu_lock_)
      menu_lock_.reset();
    overlay_view_.get().menuBarLockingEnabled = NO;
  }
  browser_view_->InvalidateLayout();
}

void ImmersiveModeControllerMac::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    browser_view_->GetWidget()->GetFocusManager()->AddFocusChangeListener(this);
    browser_view_->GetWidget()->AddObserver(this);
    browser_view_->top_container()->AddObserver(this);
    overlay_view_.reset(
        [[ImmersiveToolbarOverlayView alloc] initWithFrame:NSZeroRect]);
    menu_reveal_monitor_.reset([[MenuRevealMonitor alloc]
        initWithWindow:browser_view_->GetWidget()
                           ->GetNativeWindow()
                           .GetNativeNSWindow()
         changeHandler:^(double reveal_amount) {
           this->SetMenuRevealed(reveal_amount > 0);
         }]);
  } else {
    browser_view_->GetWidget()->GetFocusManager()->RemoveFocusChangeListener(
        this);
    browser_view_->GetWidget()->RemoveObserver(this);
    browser_view_->top_container()->RemoveObserver(this);
    [overlay_view_ removeFromSuperview];
    overlay_view_.reset();
    menu_reveal_monitor_.reset();
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

ImmersiveRevealedLock* ImmersiveModeControllerMac::GetRevealedLock(
    AnimateReveal animate_reveal) {
  revealed_lock_count_++;
  if (enabled_ && revealed_lock_count_ == 1)
    browser_view_->OnImmersiveRevealStarted();
  return new RevealedLock(weak_ptr_factory_.GetWeakPtr(), animate_reveal);
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
      focus_lock_.reset(GetRevealedLock(ANIMATE_REVEAL_YES));
  } else {
    focus_lock_.reset();
  }
}

void ImmersiveModeControllerMac::OnViewBoundsChanged(
    views::View* observed_view) {
  NSView* overlay_view = overlay_view_;
  if (observed_view->GetVisibleBounds().IsEmpty()) {
    [overlay_view removeFromSuperview];
    return;
  }
  if (!overlay_view.superview)
    [browser_view_->GetWidget()->GetNativeView().GetNativeNSView()
        addSubview:overlay_view];
  NSRect frame_rect = observed_view->bounds().ToCGRect();
  frame_rect.origin.y = NSHeight(overlay_view.superview.bounds) -
                        frame_rect.origin.y - NSHeight(frame_rect);
  overlay_view.frame = frame_rect;
}

void ImmersiveModeControllerMac::OnWidgetDestroying(views::Widget* widget) {
  SetEnabled(false);
}

void ImmersiveModeControllerMac::LockDestroyed(AnimateReveal animate_reveal) {
  revealed_lock_count_--;
  if (revealed_lock_count_ == 0)
    browser_view_->OnImmersiveRevealEnded();
}

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeControllerMac() {
  return std::make_unique<ImmersiveModeControllerMac>();
}

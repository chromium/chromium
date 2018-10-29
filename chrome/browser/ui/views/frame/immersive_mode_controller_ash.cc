// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/public/cpp/immersive/immersive_revealed_lock.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/shell.h"  // mash-ok
#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_context_mus.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_rewriter.h"
#include "ui/views/background.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

// This class rewrites located events to have no target so the target will be
// found via local process hit testing instead of the window service, which is
// unaware of the browser's top container that is on top of the web contents. An
// instance is active whenever the Mash reveal widget is active.
class LocatedEventRetargeter : public ui::EventRewriter {
 public:
  LocatedEventRetargeter() {}
  ~LocatedEventRetargeter() override {}

  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override {
    if (!event.IsLocatedEvent())
      return ui::EVENT_REWRITE_CONTINUE;

    *rewritten_event = ui::Event::Clone(event);
    // Cloning strips the EventTarget. The only goal of this EventRewriter is to
    // null the target, so there's no need to do anything extra here.
    DCHECK(!(*rewritten_event)->target());

    return ui::EVENT_REWRITE_REWRITTEN;
  }

  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override {
    return ui::EVENT_REWRITE_CONTINUE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LocatedEventRetargeter);
};

// Converts from ImmersiveModeController::AnimateReveal to
// ash::ImmersiveFullscreenController::AnimateReveal.
ash::ImmersiveFullscreenController::AnimateReveal
ToImmersiveFullscreenControllerAnimateReveal(
    ImmersiveModeController::AnimateReveal animate_reveal) {
  switch (animate_reveal) {
    case ImmersiveModeController::ANIMATE_REVEAL_YES:
      return ash::ImmersiveFullscreenController::ANIMATE_REVEAL_YES;
    case ImmersiveModeController::ANIMATE_REVEAL_NO:
      return ash::ImmersiveFullscreenController::ANIMATE_REVEAL_NO;
  }
  NOTREACHED();
  return ash::ImmersiveFullscreenController::ANIMATE_REVEAL_NO;
}

class ImmersiveRevealedLockAsh : public ImmersiveRevealedLock {
 public:
  explicit ImmersiveRevealedLockAsh(ash::ImmersiveRevealedLock* lock)
      : lock_(lock) {}

 private:
  std::unique_ptr<ash::ImmersiveRevealedLock> lock_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveRevealedLockAsh);
};

}  // namespace

ImmersiveModeControllerAsh::ImmersiveModeControllerAsh()
    : ImmersiveModeController(Type::ASH),
      controller_(std::make_unique<ash::ImmersiveFullscreenController>(
          features::IsUsingWindowService()
              ? ImmersiveContextMus::Get()
              : ash::Shell::Get()->immersive_context())),
      event_rewriter_(std::make_unique<LocatedEventRetargeter>()) {}

ImmersiveModeControllerAsh::~ImmersiveModeControllerAsh() = default;

void ImmersiveModeControllerAsh::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
  controller_->Init(this, browser_view_->frame(),
      browser_view_->top_container());

  observed_windows_.Add(
      !features::IsUsingWindowService()
          ? browser_view_->GetNativeWindow()
          : browser_view_->GetNativeWindow()->GetRootWindow());

  browser_view_->GetNativeWindow()->SetProperty(
      ash::kImmersiveWindowType,
      static_cast<int>(
          browser_view_->browser()->is_app()
              ? ash::ImmersiveFullscreenController::WINDOW_TYPE_HOSTED_APP
              : ash::ImmersiveFullscreenController::WINDOW_TYPE_BROWSER));
}

void ImmersiveModeControllerAsh::SetEnabled(bool enabled) {
  if (controller_->IsEnabled() == enabled)
    return;

  if (registrar_.IsEmpty()) {
    content::Source<FullscreenController> source(
        browser_view_->browser()
            ->exclusive_access_manager()
            ->fullscreen_controller());
    registrar_.Add(this, chrome::NOTIFICATION_FULLSCREEN_CHANGED, source);
  }

  ash::ImmersiveFullscreenController::EnableForWidget(browser_view_->frame(),
                                                      enabled);
}

bool ImmersiveModeControllerAsh::IsEnabled() const {
  return controller_->IsEnabled();
}

bool ImmersiveModeControllerAsh::ShouldHideTopViews() const {
  return controller_->IsEnabled() && !controller_->IsRevealed();
}

bool ImmersiveModeControllerAsh::IsRevealed() const {
  return controller_->IsRevealed();
}

int ImmersiveModeControllerAsh::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  if (!IsEnabled())
    return 0;

  return static_cast<int>(top_container_size.height() *
                          (visible_fraction_ - 1));
}

ImmersiveRevealedLock* ImmersiveModeControllerAsh::GetRevealedLock(
    AnimateReveal animate_reveal) {
  return new ImmersiveRevealedLockAsh(controller_->GetRevealedLock(
      ToImmersiveFullscreenControllerAnimateReveal(animate_reveal)));
}

void ImmersiveModeControllerAsh::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {
  find_bar_visible_bounds_in_screen_ = new_visible_bounds_in_screen;
}

bool ImmersiveModeControllerAsh::ShouldStayImmersiveAfterExitingFullscreen() {
  return !browser_view_->IsBrowserTypeNormal() &&
         TabletModeClient::Get()->tablet_mode_enabled();
}

void ImmersiveModeControllerAsh::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (browser_view_->IsBrowserTypeNormal())
    return;

  if (!TabletModeClient::Get()->tablet_mode_enabled())
    return;

  // Enable immersive mode if the widget is activated. Do not disable immersive
  // mode if the widget deactivates, but is not minimized.
  ash::ImmersiveFullscreenController::EnableForWidget(
      browser_view_->frame(), active || !widget->IsMinimized());
}

void ImmersiveModeControllerAsh::LayoutBrowserRootView() {
  views::Widget* widget = browser_view_->frame();
  // Update the window caption buttons.
  widget->non_client_view()->frame_view()->ResetWindowControls();
  widget->non_client_view()->frame_view()->InvalidateLayout();
  browser_view_->InvalidateLayout();
  widget->GetRootView()->Layout();
}

void ImmersiveModeControllerAsh::InstallEventRewriter() {
  if (!features::IsUsingWindowService())
    return;

  browser_view_->GetWidget()
      ->GetNativeWindow()
      ->GetHost()
      ->GetEventSource()
      ->AddEventRewriter(event_rewriter_.get());
}

void ImmersiveModeControllerAsh::UninstallEventRewriter() {
  browser_view_->GetWidget()
      ->GetNativeWindow()
      ->GetHost()
      ->GetEventSource()
      ->RemoveEventRewriter(event_rewriter_.get());
}

void ImmersiveModeControllerAsh::OnImmersiveRevealStarted() {
  UninstallEventRewriter();
  visible_fraction_ = 0;
  InstallEventRewriter();
  for (Observer& observer : observers_)
    observer.OnImmersiveRevealStarted();
}

void ImmersiveModeControllerAsh::OnImmersiveRevealEnded() {
  UninstallEventRewriter();
  visible_fraction_ = 0;
  for (Observer& observer : observers_)
    observer.OnImmersiveRevealEnded();
}

void ImmersiveModeControllerAsh::OnImmersiveFullscreenEntered() {}

void ImmersiveModeControllerAsh::OnImmersiveFullscreenExited() {
  UninstallEventRewriter();
  for (Observer& observer : observers_)
    observer.OnImmersiveFullscreenExited();
}

void ImmersiveModeControllerAsh::SetVisibleFraction(double visible_fraction) {
  if (visible_fraction_ == visible_fraction)
    return;

  visible_fraction_ = visible_fraction;
  browser_view_->Layout();
  browser_view_->frame()->GetFrameView()->UpdateClientArea();
}

std::vector<gfx::Rect>
ImmersiveModeControllerAsh::GetVisibleBoundsInScreen() const {
  views::View* top_container_view = browser_view_->top_container();
  gfx::Rect top_container_view_bounds = top_container_view->GetVisibleBounds();
  // TODO(tdanderson): Implement View::ConvertRectToScreen().
  gfx::Point top_container_view_bounds_in_screen_origin(
      top_container_view_bounds.origin());
  views::View::ConvertPointToScreen(top_container_view,
      &top_container_view_bounds_in_screen_origin);
  gfx::Rect top_container_view_bounds_in_screen(
      top_container_view_bounds_in_screen_origin,
      top_container_view_bounds.size());

  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(top_container_view_bounds_in_screen);
  bounds_in_screen.push_back(find_bar_visible_bounds_in_screen_);
  return bounds_in_screen;
}

void ImmersiveModeControllerAsh::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  if (!controller_->IsEnabled())
    return;

  // Auto hide the shelf in immersive browser fullscreen.
  bool in_tab_fullscreen = content::Source<FullscreenController>(source)->
      IsWindowFullscreenForTabOrPending();
  browser_view_->GetNativeWindow()->SetProperty(
      ash::kHideShelfWhenFullscreenKey, in_tab_fullscreen);
}

void ImmersiveModeControllerAsh::OnWindowPropertyChanged(aura::Window* window,
                                                         const void* key,
                                                         intptr_t old) {
  // Track locked fullscreen changes.
  if (key == ash::kWindowPinTypeKey) {
    browser_view_->FullscreenStateChanged();
    return;
  }

  if (key == aura::client::kShowStateKey) {
    ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);
    auto old_state = static_cast<ui::WindowShowState>(old);

    // Make sure the browser stays up to date with the window's state. This is
    // necessary in classic Ash if the user exits fullscreen with the restore
    // button, and it's necessary in OopAsh if the window manager initiates a
    // fullscreen mode change (e.g. due to a WM shortcut).
    if (new_state == ui::SHOW_STATE_FULLSCREEN ||
        old_state == ui::SHOW_STATE_FULLSCREEN) {
      // If the browser view initiated this state change,
      // BrowserView::ProcessFullscreen will no-op, so this call is harmless.
      browser_view_->FullscreenStateChanged();
    }
  }
}

void ImmersiveModeControllerAsh::OnWindowDestroying(aura::Window* window) {
  // Clean up observers here rather than in the destructor because the owning
  // BrowserView has already destroyed the aura::Window.
  observed_windows_.Remove(window);
  DCHECK(!observed_windows_.IsObservingSources());
}

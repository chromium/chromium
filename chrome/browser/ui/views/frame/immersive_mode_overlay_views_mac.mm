// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_overlay_views_mac.h"

#import <AppKit/AppKit.h>

#include <memory>
#include <set>

#include "base/memory/ptr_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_modal/browser_window_modal_dialog_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/widget/sublevel_manager.h"

// static
std::unique_ptr<OverlayWidgetMac> OverlayWidgetMac::Create(
    BrowserView* browser_view,
    views::Widget* parent) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.child = true;
  params.parent = parent->GetNativeView();
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.is_overlay = true;
  // Add kTranslucent attributes to prevent the system from adding
  // NSSheetEffectDimmingView to the overlay window when the browser displays
  // a sheet dialog, which would cause the overlay area to appear with a
  // darker dimming color on macOS.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "mac-fullscreen-overlay";
  BrowserWindowModalDialogDelegate* modal_dialog_delegate =
      BrowserWindowModalDialogDelegate::From(browser_view->browser());
  CHECK(modal_dialog_delegate);
  std::unique_ptr<OverlayWidgetMac> overlay_widget = base::WrapUnique(
      new OverlayWidgetMac(*modal_dialog_delegate, browser_view->GetWidget()));

  // When the overlay is used some Views are moved to the overlay_widget. When
  // this happens we want the fullscreen state of the overlay_widget to match
  // that of BrowserView's Widget. Without this, some views would not think
  // they are in a fullscreen Widget, when we want them to behave as though
  // they are in a fullscreen Widget.
  overlay_widget->SetCheckParentForFullscreen();

  overlay_widget->Init(std::move(params));
  // When clipsToBounds is false, an NSView can report a visibleRect with a size
  // larger than that of its bounds. We compare visibleRect and bounds sizes for
  // equality when determining whether the overlay should be moved offscreen
  // (e.g., if "Always Show Toolbar in Full Screen" is disabled), and rely on
  // these sizes being equal when the view is not obscured or otherwise hidden.
  overlay_widget->GetNativeWindow()
      .GetNativeNSWindow()
      .contentView.clipsToBounds = YES;
  overlay_widget->SetNativeWindowProperty(BrowserView::kBrowserViewKey,
                                          browser_view);

  // Disable sublevel widget layering because in fullscreen the NSWindow of
  // `overlay_widget_` is reparented to a AppKit-owned NSWindow that does not
  // have an associated Widget. This will cause issues in sublevel manager
  // which operates at the Widget level.
  if (overlay_widget->GetSublevelManager()) {
    overlay_widget->parent()->GetSublevelManager()->OnWidgetChildRemoved(
        overlay_widget->parent(), overlay_widget.get());
  }

  return overlay_widget;
}

OverlayWidgetMac::OverlayWidgetMac(
    BrowserWindowModalDialogDelegate& modal_dialog_delegate,
    views::Widget* role_model)
    : ThemeCopyingWidget(role_model),
      modal_dialog_delegate_(modal_dialog_delegate) {}

OverlayWidgetMac::~OverlayWidgetMac() = default;

void OverlayWidgetMac::OnNativeWidgetMove() {
  ThemeCopyingWidget::OnNativeWidgetMove();
  modal_dialog_delegate_->NotifyModalDialogsPositionRequiresUpdate();
}

void OverlayWidgetMac::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  ThemeCopyingWidget::OnNativeWidgetSizeChanged(new_size);
  modal_dialog_delegate_->NotifyModalDialogsPositionRequiresUpdate();
}

bool OverlayWidgetMac::GetAccelerator(int cmd_id,
                                      ui::Accelerator* accelerator) const {
  DCHECK(parent());
  return parent()->GetAccelerator(cmd_id, accelerator);
}

bool OverlayWidgetMac::ShouldViewsStyleFollowWidgetActivation() const {
  return true;
}

TabContainerOverlayViewMac::TabContainerOverlayViewMac(
    base::WeakPtr<BrowserView> browser_view)
    : browser_view_(std::move(browser_view)) {}
TabContainerOverlayViewMac::~TabContainerOverlayViewMac() = default;

void TabContainerOverlayViewMac::OnPaintBackground(gfx::Canvas* canvas) {
  SkColor frame_color =
      browser_view_->browser_widget()->GetFrameView()->GetFrameColor(
          BrowserFrameActiveState::kUseCurrent);
  canvas->DrawColor(frame_color);

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(
      browser_view_->browser()->GetProfile());
  if (!theme_service->UsingSystemTheme()) {
    auto* frame_view = static_cast<BrowserFrameViewMac*>(
        browser_view_->browser_widget()->GetFrameView());
    frame_view->PaintThemedFrame(canvas);
  }
}

//
// `BrowserRootView` handles drag and drop for the tab strip. In immersive
// fullscreen, the tab strip is hosted in a separate Widget, in a separate
// view, this view` TabContainerOverlayView`. To support drag and drop for the
// tab strip in immersive fullscreen, forward all drag and drop requests to
// the `BrowserRootView`.
//

bool TabContainerOverlayViewMac::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return browser_view_->GetWidget()->GetRootView()->GetDropFormats(
      formats, format_types);
}

bool TabContainerOverlayViewMac::AreDropTypesRequired() {
  return browser_view_->GetWidget()->GetRootView()->AreDropTypesRequired();
}

bool TabContainerOverlayViewMac::CanDrop(const ui::OSExchangeData& data) {
  return browser_view_->GetWidget()->GetRootView()->CanDrop(data);
}

void TabContainerOverlayViewMac::OnDragEntered(
    const ui::DropTargetEvent& event) {
  return browser_view_->GetWidget()->GetRootView()->OnDragEntered(event);
}

int TabContainerOverlayViewMac::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  return browser_view_->GetWidget()->GetRootView()->OnDragUpdated(event);
}

void TabContainerOverlayViewMac::OnDragExited() {
  return browser_view_->GetWidget()->GetRootView()->OnDragExited();
}

TabContainerOverlayViewMac::DropCallback
TabContainerOverlayViewMac::GetDropCallback(const ui::DropTargetEvent& event) {
  return browser_view_->GetWidget()->GetRootView()->GetDropCallback(event);
}

BEGIN_METADATA(TabContainerOverlayViewMac)
END_METADATA

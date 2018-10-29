// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/chrome_web_contents_view_delegate_mac.h"

#include <utility>

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_cocoa.h"
#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/focus_tracker.h"

ChromeWebContentsViewDelegateMac::ChromeWebContentsViewDelegateMac(
    content::WebContents* web_contents)
    : ContextMenuDelegate(web_contents),
      bookmark_handler_(new WebDragBookmarkHandlerMac),
      web_contents_(web_contents) {
}

ChromeWebContentsViewDelegateMac::~ChromeWebContentsViewDelegateMac() {
}

gfx::NativeWindow ChromeWebContentsViewDelegateMac::GetNativeWindow() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

NSObject<RenderWidgetHostViewMacDelegate>*
ChromeWebContentsViewDelegateMac::CreateRenderWidgetHostViewDelegate(
    content::RenderWidgetHost* render_widget_host,
    bool is_popup) {
  // We don't need a delegate for popups since they don't have
  // overscroll.
  if (is_popup)
    return nil;
  return [[ChromeRenderWidgetHostViewMacDelegate alloc]
      initWithRenderWidgetHost:render_widget_host];
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateMac::GetDragDestDelegate() {
  return bookmark_handler_.get();
}

void ChromeWebContentsViewDelegateMac::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  ShowMenu(
      BuildMenu(content::WebContents::FromRenderFrameHost(render_frame_host),
                params));
}

void ChromeWebContentsViewDelegateMac::StoreFocus() {
  // We're explicitly being asked to store focus, so don't worry if there's
  // already a view saved.
  focus_tracker_.reset(
      [[FocusTracker alloc] initWithWindow:GetNSWindowForFocusTracker()]);
}

bool ChromeWebContentsViewDelegateMac::RestoreFocus() {
  base::scoped_nsobject<FocusTracker> focus_tracker(std::move(focus_tracker_));

  // TODO(avi): Could we be restoring a view that's no longer in the key view
  // chain?
  if ((focus_tracker.get() &&
       [focus_tracker restoreFocusInWindow:GetNSWindowForFocusTracker()])) {
    return true;
  }

  return false;
}

void ChromeWebContentsViewDelegateMac::ResetStoredFocus() {
  focus_tracker_.reset();
}

void ChromeWebContentsViewDelegateMac::ShowMenu(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  context_menu_ = std::move(menu);
  if (!context_menu_.get())
    return;

  // The renderer may send the "show context menu" message multiple times, one
  // for each right click mouse event it receives. Normally, this doesn't happen
  // because mouse events are not forwarded once the context menu is showing.
  // However, there's a race - the context menu may not yet be showing when
  // the second mouse event arrives. In this case, |ShowContextMenu()| will
  // get called multiple times - if so, don't create another context menu.
  // TODO(asvitkine): Fix the renderer so that it doesn't do this.
  if (web_contents_->IsShowingContextMenu())
    return;

  context_menu_->Show();
}

std::unique_ptr<RenderViewContextMenuBase>
ChromeWebContentsViewDelegateMac::BuildMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  std::unique_ptr<RenderViewContextMenuBase> menu;
  menu.reset(CreateRenderViewContextMenu(web_contents, params));

  if (menu)
    menu->Init();

  return menu;
}

RenderViewContextMenuBase*
ChromeWebContentsViewDelegateMac::CreateRenderViewContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  // If the frame tree does not have a focused frame at this point, do not
  // bother creating RenderViewContextMenuBase. This happens if the frame has
  // navigated to a different page before ContextMenu message was received by
  // the current RenderFrameHost.
  content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  if (!focused_frame)
    return nullptr;

  gfx::NativeView parent_view =
      GetActiveRenderWidgetHostView()->GetNativeView();

  return new RenderViewContextMenuMacCocoa(focused_frame, params,
                                           parent_view.GetNativeNSView());
}

content::RenderWidgetHostView*
ChromeWebContentsViewDelegateMac::GetActiveRenderWidgetHostView() const {
  return web_contents_->GetFullscreenRenderWidgetHostView() ?
      web_contents_->GetFullscreenRenderWidgetHostView() :
      web_contents_->GetTopLevelRenderWidgetHostView();
}

NSWindow* ChromeWebContentsViewDelegateMac::GetNSWindowForFocusTracker() const {
  content::RenderWidgetHostView* rwhv = GetActiveRenderWidgetHostView();
  return rwhv ? [rwhv->GetNativeView().GetNativeNSView() window] : nil;
}

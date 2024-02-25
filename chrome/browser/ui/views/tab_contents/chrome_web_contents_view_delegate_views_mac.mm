// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views_mac.h"

#include <memory>

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_cocoa.h"
#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac_remote_cocoa.h"
#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_menu_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "components/remote_cocoa/browser/window.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViewsMac::ChromeWebContentsViewDelegateViewsMac(
    content::WebContents* web_contents)
    : ContextMenuDelegate(web_contents),
      web_contents_(web_contents),
      bookmark_handler_(new WebDragBookmarkHandlerMac) {
  ChromeWebContentsViewFocusHelper::CreateForWebContents(web_contents);
}

ChromeWebContentsViewDelegateViewsMac::
    ~ChromeWebContentsViewDelegateViewsMac() = default;

gfx::NativeWindow ChromeWebContentsViewDelegateViewsMac::GetNativeWindow() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

NSObject<RenderWidgetHostViewMacDelegate>*
ChromeWebContentsViewDelegateViewsMac::GetDelegateForHost(
    content::RenderWidgetHost* render_widget_host,
    bool is_popup) {
  // We don't need a delegate for popups since they don't have
  // overscroll.
  if (is_popup) {
    return nil;
  }
  return [[ChromeRenderWidgetHostViewMacDelegate alloc]
      initWithRenderWidgetHost:render_widget_host];
}

content::WebDragDestDelegate*
ChromeWebContentsViewDelegateViewsMac::GetDragDestDelegate() {
  return bookmark_handler_.get();
}

void ChromeWebContentsViewDelegateViewsMac::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  ShowMenu(BuildMenu(
      render_frame_host,
      AddContextMenuParamsPropertiesFromPreferences(web_contents_, params)));
}

void ChromeWebContentsViewDelegateViewsMac::StoreFocus() {
  GetFocusHelper()->StoreFocus();
}

bool ChromeWebContentsViewDelegateViewsMac::RestoreFocus() {
  return GetFocusHelper()->RestoreFocus();
}

void ChromeWebContentsViewDelegateViewsMac::ResetStoredFocus() {
  GetFocusHelper()->ResetStoredFocus();
}

bool ChromeWebContentsViewDelegateViewsMac::Focus() {
  return GetFocusHelper()->Focus();
}

bool ChromeWebContentsViewDelegateViewsMac::TakeFocus(bool reverse) {
  return GetFocusHelper()->TakeFocus(reverse);
}

void ChromeWebContentsViewDelegateViewsMac::OnPerformingDrop(
    const content::DropData& drop_data,
    DropCompletionCallback callback) {
  HandleOnPerformingDrop(web_contents_, drop_data, std::move(callback));
}

std::unique_ptr<RenderViewContextMenuBase>
ChromeWebContentsViewDelegateViewsMac::BuildMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  std::unique_ptr<RenderViewContextMenuMac> menu;
  if (remote_cocoa::IsWindowRemote(GetNativeWindow())) {
    menu = std::make_unique<RenderViewContextMenuMacRemoteCocoa>(
        render_frame_host, params, GetActiveRenderWidgetHostView());
  } else {
    gfx::NativeView parent_view =
        GetActiveRenderWidgetHostView()->GetNativeView();
    menu = std::make_unique<RenderViewContextMenuMacCocoa>(
        render_frame_host, params, parent_view.GetNativeNSView());
  }

  menu->Init();

  return menu;
}

void ChromeWebContentsViewDelegateViewsMac::ShowMenu(
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

content::RenderWidgetHostView*
ChromeWebContentsViewDelegateViewsMac::GetActiveRenderWidgetHostView() const {
  return web_contents_->GetTopLevelRenderWidgetHostView();
}

ChromeWebContentsViewFocusHelper*
ChromeWebContentsViewDelegateViewsMac::GetFocusHelper() const {
  ChromeWebContentsViewFocusHelper* helper =
      ChromeWebContentsViewFocusHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return helper;
}

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeWebContentsViewDelegateViewsMac>(web_contents);
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"

#include <memory>
#include <utility>

#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/aura/tab_contents/web_drag_bookmark_handler_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_menu_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/browser/ui/views/renderer_context_menu/render_view_context_menu_views.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViews::ChromeWebContentsViewDelegateViews(
    content::WebContents* web_contents)
    : ContextMenuDelegate(web_contents), web_contents_(web_contents) {
  ChromeWebContentsViewFocusHelper::CreateForWebContents(web_contents);
}

ChromeWebContentsViewDelegateViews::~ChromeWebContentsViewDelegateViews() =
    default;

gfx::NativeWindow ChromeWebContentsViewDelegateViews::GetNativeWindow() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

content::WebDragDestDelegate*
    ChromeWebContentsViewDelegateViews::GetDragDestDelegate() {
  // We install a chrome specific handler to intercept bookmark drags for the
  // bookmark manager/extension API.
  bookmark_handler_ = std::make_unique<WebDragBookmarkHandlerAura>();
  return bookmark_handler_.get();
}

ChromeWebContentsViewFocusHelper*
ChromeWebContentsViewDelegateViews::GetFocusHelper() const {
  ChromeWebContentsViewFocusHelper* helper =
      ChromeWebContentsViewFocusHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return helper;
}

bool ChromeWebContentsViewDelegateViews::Focus() {
  return GetFocusHelper()->Focus();
}

bool ChromeWebContentsViewDelegateViews::TakeFocus(bool reverse) {
  return GetFocusHelper()->TakeFocus(reverse);
}

void ChromeWebContentsViewDelegateViews::StoreFocus() {
  GetFocusHelper()->StoreFocus();
}

bool ChromeWebContentsViewDelegateViews::RestoreFocus() {
  return GetFocusHelper()->RestoreFocus();
}

void ChromeWebContentsViewDelegateViews::ResetStoredFocus() {
  GetFocusHelper()->ResetStoredFocus();
}

std::unique_ptr<RenderViewContextMenuBase>
ChromeWebContentsViewDelegateViews::BuildMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  std::unique_ptr<RenderViewContextMenuBase> menu(
      RenderViewContextMenuViews::Create(render_frame_host, params));
  menu->Init();
  return menu;
}

void ChromeWebContentsViewDelegateViews::ShowMenu(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  context_menu_ = std::move(menu);
  if (!context_menu_)
    return;

  context_menu_->Show();
}

void ChromeWebContentsViewDelegateViews::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  ShowMenu(BuildMenu(
      render_frame_host,
      AddContextMenuParamsPropertiesFromPreferences(web_contents_, params)));
}

void ChromeWebContentsViewDelegateViews::ExecuteCommandForTesting(
    int command_id,
    int event_flags) {
  DCHECK(context_menu_);
  context_menu_->ExecuteCommand(command_id, event_flags);
  context_menu_.reset();
}

void ChromeWebContentsViewDelegateViews::OnPerformingDrop(
    const content::DropData& drop_data,
    DropCompletionCallback callback) {
  HandleOnPerformingDrop(web_contents_, drop_data, std::move(callback));
}

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeWebContentsViewDelegateViews>(web_contents);
}

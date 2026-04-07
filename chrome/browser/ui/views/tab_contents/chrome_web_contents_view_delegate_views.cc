// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"

#include <memory>
#include <utility>

#include "chrome/browser/defaults.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
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
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViews::ChromeWebContentsViewDelegateViews(
    content::WebContents* web_contents)
    : ContextMenuDelegate(web_contents), web_contents_(web_contents) {
  ChromeWebContentsViewFocusHelper::CreateForWebContents(web_contents);
}

ChromeWebContentsViewDelegateViews::~ChromeWebContentsViewDelegateViews() =
    default;

gfx::NativeWindow ChromeWebContentsViewDelegateViews::GetNativeWindow() {
  BrowserWindowInterface* browser = chrome::FindBrowserWithTab(web_contents_);
  return browser ? browser->GetWindow()->GetNativeWindow() : nullptr;
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
      RenderViewContextMenuViews::Create(render_frame_host, params,
                                         is_paste_enabled_,
                                         is_paste_and_match_style_enabled_));
  menu->Init();
  return menu;
}

void ChromeWebContentsViewDelegateViews::ShowMenu(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  context_menu_ = std::move(menu);
  if (!context_menu_) {
    return;
  }

  context_menu_->Show();
}

void ChromeWebContentsViewDelegateViews::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  std::optional<ui::DataTransferEndpoint> data_dst;
  if (params.page_url.is_valid()) {
    data_dst.emplace(
        params.page_url,
        ui::DataTransferEndpointOptions{
            .notify_if_restricted = false,
            .off_the_record =
                web_contents_->GetBrowserContext()->IsOffTheRecord(),
        });
  }
  ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(
          &ChromeWebContentsViewDelegateViews::OnReadAvailableTypes,
          weak_ptr_factory_.GetWeakPtr(), render_frame_host.GetGlobalId(),
          AddContextMenuParamsPropertiesFromPreferences(web_contents_, params),
          data_dst));
}

void ChromeWebContentsViewDelegateViews::OnReadAvailableTypes(
    content::GlobalRenderFrameHostId render_frame_host_id,
    const content::ContextMenuParams& params,
    std::optional<ui::DataTransferEndpoint> data_dst,
    std::vector<std::u16string> types) {
  is_paste_enabled_ = !types.empty();

  ui::Clipboard::GetForCurrentThread()->GetAllAvailableFormats(
      ui::ClipboardBuffer::kCopyPaste, std::move(data_dst),
      base::BindOnce(
          &ChromeWebContentsViewDelegateViews::OnGetAllAvailableFormats,
          weak_ptr_factory_.GetWeakPtr(), render_frame_host_id, params));
}

void ChromeWebContentsViewDelegateViews::OnGetAllAvailableFormats(
    content::GlobalRenderFrameHostId render_frame_host_id,
    const content::ContextMenuParams& params,
    base::flat_set<ui::ClipboardFormatType> formats) {
  is_paste_and_match_style_enabled_ =
      formats.contains(ui::ClipboardFormatType::PlainTextType());

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);
  if (!render_frame_host) {
    return;
  }

  ShowMenu(BuildMenu(*render_frame_host, params));
}

void ChromeWebContentsViewDelegateViews::ExecuteCommandForTesting(
    int command_id,
    int event_flags) {
  DCHECK(context_menu_);
  context_menu_->ExecuteCommand(command_id, event_flags);
  context_menu_.reset();
}

bool ChromeWebContentsViewDelegateViews::IsContextMenuShowingForTesting() {
  return !!context_menu_;
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

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsEscapeEvent(const content::NativeWebKeyboardEvent& event) {
  return event.GetType() ==
             content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

content::WebContents::CreateParams GetWebContentsCreateParams(
    content::BrowserContext* browser_context,
    const GURL& webui_url) {
  content::WebContents::CreateParams create_params(browser_context);
  create_params.initially_hidden = true;
  create_params.site_instance =
      content::SiteInstance::CreateForURL(browser_context, webui_url);

  return create_params;
}

}  // namespace

bool BubbleContentsWrapper::Host::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

bool BubbleContentsWrapper::Host::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

content::WebContents* BubbleContentsWrapper::Host::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return nullptr;
}

BubbleContentsWrapper::BubbleContentsWrapper(
    const GURL& webui_url,
    content::BrowserContext* browser_context,
    int task_manager_string_id,
    bool webui_resizes_host,
    bool esc_closes_ui)
    : webui_resizes_host_(webui_resizes_host),
      esc_closes_ui_(esc_closes_ui),
      web_contents_(content::WebContents::Create(
          GetWebContentsCreateParams(browser_context, webui_url))) {
  web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(web_contents_.get());

  PrefsTabHelper::CreateForWebContents(web_contents_.get());
  task_manager::WebContentsTags::CreateForToolContents(web_contents_.get(),
                                                       task_manager_string_id);
}

BubbleContentsWrapper::~BubbleContentsWrapper() {
  WebContentsObserver::Observe(nullptr);
}

void BubbleContentsWrapper::ResizeDueToAutoResize(content::WebContents* source,
                                                  const gfx::Size& new_size) {
  DCHECK_EQ(web_contents(), source);
  if (host_)
    host_->ResizeDueToAutoResize(source, new_size);
}

content::KeyboardEventProcessingResult
BubbleContentsWrapper::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(web_contents(), source);
  // Close the bubble if an escape event is detected. Handle this here to
  // prevent the renderer from capturing the event and not propagating it up.
  if (host_ && IsEscapeEvent(event) && esc_closes_ui_) {
    host_->CloseUI();
    return content::KeyboardEventProcessingResult::HANDLED;
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BubbleContentsWrapper::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(web_contents(), source);
  return host_ ? host_->HandleKeyboardEvent(source, event) : false;
}

bool BubbleContentsWrapper::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return host_ ? host_->HandleContextMenu(render_frame_host, params) : true;
}

std::unique_ptr<content::EyeDropper> BubbleContentsWrapper::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  BrowserWindow* window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents_.get());
  return window->OpenEyeDropper(frame, listener);
}

content::WebContents* BubbleContentsWrapper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return host_ ? host_->OpenURLFromTab(source, params) : nullptr;
}

void BubbleContentsWrapper::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (host_) {
    host_->RequestMediaAccessPermission(web_contents, request,
                                        std::move(callback));
  }
}

void BubbleContentsWrapper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  if (host_) {
    host_->RunFileChooser(render_frame_host, listener, params);
  }
}

void BubbleContentsWrapper::PrimaryPageChanged(content::Page& page) {
  content::RenderWidgetHostView* render_widget_host_view =
      web_contents_->GetRenderWidgetHostView();
  if (!webui_resizes_host_ || !render_widget_host_view)
    return;

  render_widget_host_view->EnableAutoResize(gfx::Size(1, 1),
                                            gfx::Size(INT_MAX, INT_MAX));
}

void BubbleContentsWrapper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CloseUI();
}

void BubbleContentsWrapper::ShowUI() {
  if (host_)
    host_->ShowUI();
}

void BubbleContentsWrapper::CloseUI() {
  if (host_)
    host_->CloseUI();
}

void BubbleContentsWrapper::ShowContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  if (host_)
    host_->ShowCustomContextMenu(point, std::move(menu_model));
}

void BubbleContentsWrapper::HideContextMenu() {
  if (host_)
    host_->HideCustomContextMenu();
}

base::WeakPtr<BubbleContentsWrapper::Host> BubbleContentsWrapper::GetHost() {
  return host_;
}

void BubbleContentsWrapper::SetHost(
    base::WeakPtr<BubbleContentsWrapper::Host> host) {
  DCHECK(!web_contents_->IsCrashed());
  host_ = std::move(host);
}

void BubbleContentsWrapper::SetWebContentsForTesting(
    std::unique_ptr<content::WebContents> web_contents) {
  web_contents_->SetDelegate(nullptr);
  web_contents_ = std::move(web_contents);
  web_contents_->SetDelegate(this);
}

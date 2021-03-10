// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"

#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsEscapeEvent(const content::NativeWebKeyboardEvent& event) {
  return event.GetType() ==
             content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

content::WebContents::CreateParams GetWebContentsCreateParams(
    content::BrowserContext* browser_context) {
  content::WebContents::CreateParams create_params(browser_context);
  create_params.initially_hidden = true;
  return create_params;
}

}  // namespace

bool BubbleContentsWrapper::Host::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

BubbleContentsWrapper::BubbleContentsWrapper(
    content::BrowserContext* browser_context,
    int task_manager_string_id,
    bool enable_extension_apis,
    bool webui_resizes_host)
    : webui_resizes_host_(webui_resizes_host),
      web_contents_(content::WebContents::Create(
          GetWebContentsCreateParams(browser_context))) {
  web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(web_contents_.get());

  if (enable_extension_apis) {
    // In order for the WebUI in the renderer to use extensions APIs we must
    // add a ChromeExtensionWebContentsObserver to the WebView's WebContents.
    extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
        web_contents_.get());
  }
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
  if (host_ && IsEscapeEvent(event)) {
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
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void BubbleContentsWrapper::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  content::RenderWidgetHostView* render_widget_host_view =
      web_contents_->GetRenderWidgetHostView();
  if (!webui_resizes_host_ || !render_widget_host_view)
    return;

  render_widget_host_view->EnableAutoResize(gfx::Size(1, 1),
                                            gfx::Size(INT_MAX, INT_MAX));
}

void BubbleContentsWrapper::RenderProcessGone(base::TerminationStatus status) {
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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"

#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsEscapeEvent(const content::NativeWebKeyboardEvent& event) {
  return event.GetType() ==
             content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

// The min / max size available to the WebUIBubbleView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// WebUIBubbleView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

}  // namespace

WebUIBubbleView::WebUIBubbleView(content::BrowserContext* browser_context)
    : WebView(browser_context) {
  EnableSizingFromWebContents(kMinSize, kMaxSize);
  SetVisible(false);

  // Allow the embedder to handle accelerators not handled by the WebContents.
  set_allow_accelerators(true);
}

WebUIBubbleView::~WebUIBubbleView() = default;

void WebUIBubbleView::PreferredSizeChanged() {
  WebView::PreferredSizeChanged();
  if (host_)
    host_->OnWebViewSizeChanged();
}

bool WebUIBubbleView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

content::KeyboardEventProcessingResult WebUIBubbleView::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Close the bubble if an escape event is detected. Handle this here to
  // prevent the renderer from capturing the event and not propagating it up.
  if (IsEscapeEvent(event) && GetWidget()) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
    return content::KeyboardEventProcessingResult::HANDLED;
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebUIBubbleView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void WebUIBubbleView::ShowUI() {
  if (host_)
    host_->ShowUI();
}

void WebUIBubbleView::CloseUI() {
  if (host_)
    host_->CloseUI();
}

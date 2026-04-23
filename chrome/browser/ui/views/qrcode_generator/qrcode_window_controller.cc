// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_window_controller.h"

#include <utility>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/action_id.h"

namespace qrcode_generator {

DEFINE_USER_DATA(QRCodeWindowController);

// static
QRCodeWindowController* QRCodeWindowController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

QRCodeWindowController::QRCodeWindowController(BrowserWindowInterface* browser)
    : browser_(*browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {}

QRCodeWindowController::~QRCodeWindowController() = default;

QRCodeGeneratorBubbleView* QRCodeWindowController::ShowBubble(
    content::WebContents* contents,
    const GURL& url,
    bool show_back_button) {
  auto* controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(contents);
  base::OnceClosure on_closing = controller->GetOnBubbleClosedCallback();
  base::OnceClosure on_back_button_pressed;
  if (show_back_button) {
    on_back_button_pressed = controller->GetOnBackButtonPressedCallback();
  }

  auto anchor = ToolbarButtonProvider::From(&*browser_)
                    ->GetBubbleAnchor(kActionQrCodeGenerator);

  auto bubble = std::make_unique<qrcode_generator::QRCodeGeneratorBubble>(
      anchor, contents->GetWeakPtr(), std::move(on_closing),
      std::move(on_back_button_pressed), url);
  auto* bubble_ptr = bubble.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  bubble_ptr->Show();

  return bubble_ptr;
}

}  // namespace qrcode_generator

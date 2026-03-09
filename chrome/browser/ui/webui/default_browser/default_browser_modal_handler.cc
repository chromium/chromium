// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/display/screen.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

DefaultBrowserModalHandler::DefaultBrowserModalHandler(
    content::WebUI* web_ui,
    mojo::PendingReceiver<default_browser_modal::mojom::PageHandler> receiver)
    : web_ui_(web_ui), receiver_(this, std::move(receiver)) {}

DefaultBrowserModalHandler::~DefaultBrowserModalHandler() = default;

void DefaultBrowserModalHandler::Cancel() {
  auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
  if (auto* surface_manager = prompt_manager->GetPromptSurfaceManager()) {
    surface_manager->HandleDismiss();
    prompt_manager->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
  }

  if (web_ui_ && web_ui_->GetWebContents()) {
    web_ui_->GetWebContents()->Close();
  }
}

void DefaultBrowserModalHandler::Confirm() {
  auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
  if (auto* surface_manager = prompt_manager->GetPromptSurfaceManager()) {
    surface_manager->HandleAccept();
    prompt_manager->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);
  }

  if (web_ui_ && web_ui_->GetWebContents()) {
    web_ui_->GetWebContents()->Close();
  }
}

void DefaultBrowserModalHandler::ContentReady(uint32_t content_height) {
  if (!web_ui_ || !web_ui_->GetWebContents()) {
    return;
  }

  gfx::NativeWindow native_window =
      web_ui_->GetWebContents()->GetTopLevelNativeWindow();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window);
  if (!widget) {
    return;
  }

  display::Display display =
      display::Screen::Get()->GetDisplayNearestWindow(native_window);
  // Dialog height cannot be bigger than the screen height.
  const auto screen_height =
      static_cast<uint32_t>(display.work_area().height());

  if (content_height < 0 || content_height > screen_height) {
    return;
  }

  if (auto* delegate = widget->widget_delegate()->AsDialogDelegate()) {
    if (auto* dialog = static_cast<default_browser::DefaultBrowserModalDialog*>(
            delegate)) {
      dialog->ResizeNativeViewHeight(content_height);

      CHECK(!widget->IsVisible());
      widget->Show();
    }
  }
}

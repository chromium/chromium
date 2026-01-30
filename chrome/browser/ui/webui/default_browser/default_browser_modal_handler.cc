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
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

DefaultBrowserModalHandler::DefaultBrowserModalHandler(
    content::WebUI* web_ui,
    mojo::PendingReceiver<default_browser_modal::mojom::PageHandler> receiver)
    : web_ui_(web_ui), receiver_(this, std::move(receiver)) {
  controller_ = default_browser::DefaultBrowserManager::CreateControllerFor(
      default_browser::DefaultBrowserEntrypointType::kSettingsPage);
  if (controller_) {
    controller_->OnShown();
  }
}

DefaultBrowserModalHandler::~DefaultBrowserModalHandler() {
  if (controller_) {
    controller_->OnIgnored();
  }
}

void DefaultBrowserModalHandler::Cancel() {
  if (auto controller = std::exchange(controller_, nullptr)) {
    controller->OnDismissed();
  }

  if (web_ui_ && web_ui_->GetWebContents()) {
    web_ui_->GetWebContents()->Close();
  }
}

void DefaultBrowserModalHandler::Confirm() {
  if (auto controller = std::exchange(controller_, nullptr)) {
    controller->OnAccepted(base::DoNothingWithBoundArgs(std::move(controller)));
  }

  if (web_ui_ && web_ui_->GetWebContents()) {
    web_ui_->GetWebContents()->Close();
  }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"

#include <utility>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"

ReloadButtonPageHandler::ReloadButtonPageHandler(
    mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver,
    mojo::PendingRemote<reload_button::mojom::Page> page,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      command_updater_(webui::GetBrowserWindowInterface(web_contents)
                           ->GetFeatures()
                           .browser_command_controller()) {}

ReloadButtonPageHandler::~ReloadButtonPageHandler() = default;

void ReloadButtonPageHandler::Reload() {
  command_updater_->ExecuteCommand(IDC_RELOAD);
}

void ReloadButtonPageHandler::StopReload() {
  command_updater_->ExecuteCommand(IDC_STOP);
}

void ReloadButtonPageHandler::SetLoadingState(bool is_loading, bool force) {
  if (page_) {
    page_->SetLoadingState(is_loading);
  }
}

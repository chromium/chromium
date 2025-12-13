// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/main/send_search_feedback_handler.h"

#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

SendSearchFeedbackHandler::SendSearchFeedbackHandler() = default;

SendSearchFeedbackHandler::~SendSearchFeedbackHandler() = default;

void SendSearchFeedbackHandler::OnJavascriptAllowed() {}

void SendSearchFeedbackHandler::OnJavascriptDisallowed() {}

void SendSearchFeedbackHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openSearchFeedbackDialog",
      base::BindRepeating(&SendSearchFeedbackHandler::HandleOpenFeedbackDialog,
                          base::Unretained(this)));
}

void SendSearchFeedbackHandler::OpenFeedbackDialogWrapper(
    const std::string& description_template) {
  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetBrowserForTab(
          web_ui()->GetWebContents());
  if (browser) {
    chrome::OpenFeedbackDialog(&browser->GetBrowser(),
                               feedback::kFeedbackSourceOsSettingsSearch,
                               description_template);
  }
}

void SendSearchFeedbackHandler::HandleOpenFeedbackDialog(
    const base::Value::List& args) {
  DCHECK_EQ(args.size(), 1U);
  OpenFeedbackDialogWrapper(args.front().GetString());
}
}  // namespace ash::settings

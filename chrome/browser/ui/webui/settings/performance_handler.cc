// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/performance_handler.h"
#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "content/public/browser/web_ui.h"

namespace settings {

PerformanceHandler::PerformanceHandler() = default;
PerformanceHandler::~PerformanceHandler() = default;

void PerformanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openHighEfficiencyFeedbackDialog",
      base::BindRepeating(
          &PerformanceHandler::HandleOpenHighEfficiencyFeedbackDialog,
          base::Unretained(this)));
}

void PerformanceHandler::HandleOpenHighEfficiencyFeedbackDialog(
    const base::Value::List& args) {
  HandleOpenFeedbackDialog("performance_tabs");
}

void PerformanceHandler::HandleOpenFeedbackDialog(
    const std::string category_tag) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  DCHECK(browser);
  std::string unused;
  chrome::ShowFeedbackPage(browser,
                           chrome::kFeedbackSourceSettingsPerformancePage,
                           unused, unused, category_tag, unused);
}

}  // namespace settings

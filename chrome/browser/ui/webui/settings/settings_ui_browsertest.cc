// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

typedef InProcessBrowserTest SettingsUITest;

using ui_test_utils::NavigateToURL;

IN_PROC_BROWSER_TEST_F(SettingsUITest, ViewSourceDoesntCrash) {
  NavigateToURL(browser(),
                GURL(content::kViewSourceScheme + std::string(":") +
                     chrome::kChromeUISettingsURL + std::string("strings.js")));
}

// Catch lifetime issues in message handlers. There was previously a problem
// with PrefMember calling Init again after Destroy.
IN_PROC_BROWSER_TEST_F(SettingsUITest, ToggleJavaScript) {
  NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL));

  const auto& handlers = *browser()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetWebUI()
                              ->GetHandlersForTesting();

  for (const std::unique_ptr<content::WebUIMessageHandler>& handler :
       handlers) {
    handler->AllowJavascriptForTesting();
    handler->DisallowJavascript();
    handler->AllowJavascriptForTesting();
  }
}

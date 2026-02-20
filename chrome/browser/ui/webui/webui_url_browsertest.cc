// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"

using WebUIUrlBrowserTest = InProcessBrowserTest;

// Tests that all registered WebUIs have their URL listed in kChromeUrls or in
// kChromeUntestedUrls, so that basic sanity checks can run on them.
IN_PROC_BROWSER_TEST_F(WebUIUrlBrowserTest, UrlsInTestList) {
  content::WebUIConfigMap& map = content::WebUIConfigMap::GetInstance();
  std::set<std::string> missing_entries;
  for (const content::WebUIConfigInfo& config_info :
       map.GetWebUIConfigList(nullptr)) {
    std::string url = config_info.origin.Serialize();
    missing_entries.insert(url);
  }

  for (const char* url : kChromeUrls) {
    missing_entries.erase(url);
  }

  for (const char* url : kChromeUntestedUrls) {
    missing_entries.erase(url);
  }

  EXPECT_TRUE(missing_entries.empty())
      << "Please add this URL to kChromeUrls in "
         "//chrome/browser/ui/webui/webui_urls_for_test.h:"
      << std::endl
      << base::JoinString(
             std::vector(missing_entries.begin(), missing_entries.end()), "\n");
}

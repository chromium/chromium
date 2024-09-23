// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/hash.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/webui/webui_url_hashes.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"

using WebUIUrlHashesBrowserTest = InProcessBrowserTest;

// Tests that the URL hashes are listed in enum WebUIUrlHashes from
// //tools/metrics/histograms/enums.xml. Not finding a URL will cause a CHECK
// failure. enum WebUIUrlHashes is used to collect WebUI usage statistics.
// The URL hash is recorded in histogram "WebUI.CreatedForUrl" when a WebUI
// is opened.
IN_PROC_BROWSER_TEST_F(WebUIUrlHashesBrowserTest, UrlsInHistogram) {
  content::WebUIConfigMap& map = content::WebUIConfigMap::GetInstance();
  std::vector<std::string> missing_entries;
  for (const content::WebUIConfigInfo& config_info :
       map.GetWebUIConfigList(nullptr)) {
    std::string url = config_info.origin.Serialize() + "/";
    uint32_t hash = base::Hash(url);
    std::string hash_string =
        base::NumberToString(static_cast<base::HistogramBase::Sample>(hash));
    if (!webui_metrics::IsValidWebUIUrlHashes(hash_string)) {
      missing_entries.push_back(base::StrCat(
          {"  <int value=\"", hash_string, "\" label=\"", url, "\"/>"}));
    }
  }
  EXPECT_TRUE(missing_entries.empty())
      << "Please add this line to enum WebUIUrlHashes in "
         "//tools/metrics/histograms/enums.xml:"
      << std::endl
      << base::JoinString(missing_entries, "\n");
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_url_hashes.h"

#include "base/hash/hash.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/webui/webui_hosts.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/internal_webui_config.h"
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
        base::NumberToString(static_cast<base::HistogramBase::Sample32>(hash));
    if (!webui_metrics::IsValidWebUIUrlHashes(hash_string)) {
      missing_entries.push_back(base::StrCat(
          {"  <int value=\"", hash_string, "\" label=\"", url, "\"/>"}));
    }
  }
  EXPECT_TRUE(missing_entries.empty())
      << "Please add this line to enum WebUIUrlHashes in "
         "//tools/metrics/histograms/metadata/ui/enums.xml:"
      << std::endl
      << base::JoinString(missing_entries, "\n");
}

// Tests that the URL names are listed in variants WebUIHost from
// //tools/metrics/histograms/metadata/page/histograms.xml. Not finding a URL
// will cause a CHECK failure. The variant, WebUIHost, is used to collect WebUI
// performance statistics and is used in histograms
// "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.WebUI{WebUIHost}" and
// "PageLoad.PaintTiming.NavigationToLargestContentfulPaint.WebUI{WebUIHost}"
// when a WebUI is viewed.
IN_PROC_BROWSER_TEST_F(WebUIUrlHashesBrowserTest, HostsInHistogram) {
  content::WebUIConfigMap& map = content::WebUIConfigMap::GetInstance();
  std::vector<std::string> missing_entries;
  for (const content::WebUIConfigInfo& config_info :
       map.GetWebUIConfigList(nullptr)) {
    GURL url = config_info.origin.GetURL();
    std::string host_variant = base::StrCat({".", url.host()});
    if (!content::IsInternalWebUI(url) &&
        !webui_metrics::IsValidWebUIHost(host_variant)) {
      missing_entries.push_back(
          base::StrCat({"  <variant name=\"", host_variant, "\"/>"}));
    }
  }
  EXPECT_TRUE(missing_entries.empty())
      << "Please add this line to variant WebUIHost in "
         "//tools/metrics/histograms/metadata/page/histograms.xml:"
      << std::endl
      << base::JoinString(missing_entries, "\n");
}

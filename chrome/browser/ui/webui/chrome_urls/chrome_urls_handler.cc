// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_handler.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "url/gurl.h"

namespace chrome_urls {

ChromeUrlsHandler::ChromeUrlsHandler(
    mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver,
    mojo::PendingRemote<chrome_urls::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {
  DCHECK(base::FeatureList::IsEnabled(features::kInternalOnlyUisPref));
}

ChromeUrlsHandler::~ChromeUrlsHandler() = default;

void ChromeUrlsHandler::GetUrls(GetUrlsCallback callback) {
  // TODO (crbug.com/379889249): Replace all of this dummy code with the real
  // implementation.
  std::vector<chrome_urls::mojom::WebuiUrlInfoPtr> webui_urls;
  webui_urls.reserve(3);
  const GURL test_urls[] = {GURL("chrome://settings"),
                            GURL("chrome://bookmarks"),
                            GURL("chrome://downloads")};
  for (const GURL& url : test_urls) {
    chrome_urls::mojom::WebuiUrlInfoPtr info(
        chrome_urls::mojom::WebuiUrlInfo::New());
    info->url = url;
    info->enabled = true;
    webui_urls.push_back(std::move(info));
  }
  chrome_urls::mojom::ChromeUrlsDataPtr dummy_result(
      chrome_urls::mojom::ChromeUrlsData::New());
  dummy_result->webui_urls = std::move(webui_urls);
  dummy_result->command_urls = {GURL("chrome://crash"), GURL("chrome://kill"),
                                GURL("chrome://hang")};
  std::move(callback).Run(std::move(dummy_result));
}

}  // namespace chrome_urls

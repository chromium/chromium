// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_MANAGER_TEST_API_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_MANAGER_TEST_API_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace webui {
class PreloadCandidateSelector;
}  // namespace webui

class WebUIContentsPreloadManagerTestAPI {
 public:
  WebUIContentsPreloadManagerTestAPI() = default;

  std::vector<GURL> GetAllPreloadableWebUIURLs();

  std::optional<GURL> GetPreloadedURL();

  std::optional<GURL> GetNextWebUIURLToPreload(
      content::BrowserContext* browser_context);

  void MaybePreloadForBrowserContext(content::BrowserContext* browser_context);

  void MaybePreloadForBrowserContextLater(
      content::BrowserContext* browser_context,
      content::WebContents* busy_web_contents_to_watch,
      base::TimeDelta deadline);

  void SetPreloadedContents(std::unique_ptr<content::WebContents> web_contents);

  void SetPreloadCandidateSelector(
      std::unique_ptr<webui::PreloadCandidateSelector>
          preload_candidate_selector);

 private:
  WebUIContentsPreloadManager* preload_manager() {
    return WebUIContentsPreloadManager::GetInstance();
  }
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_MANAGER_TEST_API_H_

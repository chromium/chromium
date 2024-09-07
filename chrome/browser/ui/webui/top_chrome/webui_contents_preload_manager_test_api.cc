// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"

#include <vector>

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "url/gurl.h"

std::vector<GURL>
WebUIContentsPreloadManagerTestAPI::GetAllPreloadableWebUIURLs() {
  return preload_manager()->GetAllPreloadableWebUIURLs();
}

std::optional<GURL> WebUIContentsPreloadManagerTestAPI::GetPreloadedURL() {
  if (content::WebContents* preloaded_web_contents =
          preload_manager()->preloaded_web_contents()) {
    return preloaded_web_contents->GetVisibleURL();
  }
  return std::nullopt;
}

std::optional<GURL>
WebUIContentsPreloadManagerTestAPI::GetNextWebUIURLToPreload(
    content::BrowserContext* browser_context) {
  return preload_manager()->GetNextWebUIURLToPreload(browser_context);
}

void WebUIContentsPreloadManagerTestAPI::MaybePreloadForBrowserContext(
    content::BrowserContext* browser_context) {
  return preload_manager()->MaybePreloadForBrowserContext(browser_context);
}

void WebUIContentsPreloadManagerTestAPI::MaybePreloadForBrowserContextLater(
    content::BrowserContext* browser_context,
    content::WebContents* busy_web_contents_to_watch,
    base::TimeDelta deadline) {
  return preload_manager()->MaybePreloadForBrowserContextLater(
      browser_context, busy_web_contents_to_watch, deadline);
}

void WebUIContentsPreloadManagerTestAPI::SetPreloadedContents(
    std::unique_ptr<content::WebContents> web_contents) {
  preload_manager()->SetPreloadedContents(std::move(web_contents));
}

void WebUIContentsPreloadManagerTestAPI::SetPreloadCandidateSelector(
    std::unique_ptr<webui::PreloadCandidateSelector>
        preload_candidate_selector) {
  return preload_manager()->SetPreloadCandidateSelector(
      std::move(preload_candidate_selector));
}

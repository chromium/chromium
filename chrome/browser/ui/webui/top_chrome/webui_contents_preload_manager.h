// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_MANAGER_H_

#include "base/callback_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// This is a singleton class that preloads Top Chrome WebUIs resources.
// If preloaded, it hosts a WebContents that can later be used to show a WebUI.
// The currently implementation preloads Tab Search. If a different WebUI
// is requested, it redirects the preloaded WebContents to the requested one.
// If under heavy memory pressure, no preloaded contents will be created.
class WebUIContentsPreloadManager {
 public:
  WebUIContentsPreloadManager();
  ~WebUIContentsPreloadManager();

  WebUIContentsPreloadManager(const WebUIContentsPreloadManager&) = delete;
  WebUIContentsPreloadManager& operator=(const WebUIContentsPreloadManager&) =
      delete;

  static WebUIContentsPreloadManager* GetInstance();

  // Preload a WebContents for `browser_context`.
  // There is at most one preloaded contents at any time.
  // If the preloaded contents has a different browser context, it will be
  // replaced with a new contents under the given `browser_context`.
  // If under heavy memory pressure, no preloaded contents will be created.
  void PreloadForBrowserContext(content::BrowserContext* browser_context);

  // Make a WebContents that shows `webui_url` under `browser_context`.
  // Reuses the preloaded contents if it is under the same `browser_context`.
  // A new preloaded contents will be created, unless we are under heavy
  // memory pressure.
  std::unique_ptr<content::WebContents> MakeContents(
      const GURL& webui_url,
      content::BrowserContext* browser_context);

  content::WebContents* preloaded_web_contents_for_testing() {
    return preloaded_web_contents_.get();
  }

  GURL GetPreloadedURLForTesting() const;

 private:
  static const char* const kPreloadedWebUIURL;

  std::unique_ptr<content::WebContents> CreateNewContents(
      content::BrowserContext* browser_context,
      GURL url = GURL(kPreloadedWebUIURL));

  // Returns true if a new preloaded contents should be created for
  // `browser_context`.
  bool ShouldPreloadForBrowserContext(
      content::BrowserContext* browser_context) const;

  // Cleans up preloaded contents on browser context shutdown.
  void OnBrowserContextShutdown(content::BrowserContext* browser_context);

  std::unique_ptr<content::WebContents> preloaded_web_contents_;

  base::CallbackListSubscription browser_context_shutdown_subscription_;
};

#endif  //  CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_MANAGER_H_

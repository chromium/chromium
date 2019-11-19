// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_URL_LOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_URL_LOADER_H_

#include "base/callback.h"
#include "base/time/time.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Callback-based wrapper around NavigationController::LoadUrl.
class WebAppUrlLoader {
 public:
  // How to compare resolved URL against the given URL for the purpose of
  // determining a successful load.
  enum class UrlComparison {
    kExact,
    kIgnoreQueryParamsAndRef,
    kSameOrigin,
  };

  enum class Result {
    // The provided URL (matched using |UrlComparison|) was loaded.
    kUrlLoaded,
    // The provided URL redirected to another URL (that did not match using
    // |UrlComparison|) and the final URL was loaded.
    kRedirectedUrlLoaded,
    kFailedUnknownReason,
    kFailedPageTookTooLong,
    kFailedWebContentsDestroyed,
  };

  using ResultCallback = base::OnceCallback<void(Result)>;

  WebAppUrlLoader();
  virtual ~WebAppUrlLoader();

  // Navigates |web_contents| to |url|, compares the resolved URL with
  // |url_comparison|, and runs callback with the result code.
  virtual void LoadUrl(const GURL& url,
                       content::WebContents* web_contents,
                       UrlComparison url_comparison,
                       ResultCallback callback);

  // Exposed for testing.
  static constexpr base::TimeDelta kSecondsToWaitForWebContentsLoad =
      base::TimeDelta::FromSeconds(30);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_URL_LOADER_H_

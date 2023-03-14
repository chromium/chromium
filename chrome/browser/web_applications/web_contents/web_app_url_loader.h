// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_URL_LOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_URL_LOADER_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Result enum values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused. Update corresponding enums.xml
// entry when making changes here.
enum class WebAppUrlLoaderResult {
  // The provided URL (matched using |UrlComparison|) was loaded.
  kUrlLoaded = 0,
  // The provided URL redirected to another URL (that did not match using
  // |UrlComparison|) and the final URL was loaded.
  kRedirectedUrlLoaded = 1,
  kFailedUnknownReason = 2,
  kFailedPageTookTooLong = 3,
  kFailedWebContentsDestroyed = 4,
  kFailedErrorPageLoaded = 5,

  kMaxValue = kFailedErrorPageLoaded,
};

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

  using Result = WebAppUrlLoaderResult;

  using ResultCallback = base::OnceCallback<void(Result)>;

  WebAppUrlLoader();
  virtual ~WebAppUrlLoader();

  // Navigates |web_contents| to about:blank to prepare for the next LoadUrl()
  // call.
  //
  // We've observed some races when using LoadUrl() on previously navigated
  // WebContents. Sometimes events from the last navigation are triggered after
  // we start the new navigation, causing us to incorrectly run the callback
  // with a redirect error.
  //
  // Clients of LoadUrl() should always call PrepareForLoad() before calling
  // LoadUrl(). PrepareForLoad() will start a new navigation to about:blank and
  // ignore all navigation events until we've successfully navigated to
  // about:blank or timed out.
  //
  // Clients should check |callback| result and handle failure scenarios
  // appropriately.
  virtual void PrepareForLoad(content::WebContents* web_contents,
                              ResultCallback callback);

  // Navigates |web_contents| to |url|, compares the resolved URL with
  // |url_comparison|, and runs callback with the result code.
  virtual void LoadUrl(const GURL& url,
                       content::WebContents* web_contents,
                       UrlComparison url_comparison,
                       ResultCallback callback);

  // Exposed for testing.
  static constexpr base::TimeDelta kSecondsToWaitForWebContentsLoad =
      base::Seconds(30);
};

const char* ConvertUrlLoaderResultToString(WebAppUrlLoader::Result result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_URL_LOADER_H_

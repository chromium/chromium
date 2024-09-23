// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_URL_LOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_URL_LOADER_H_

#include <map>
#include <queue>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "url/gurl.h"

namespace web_app {

class TestWebAppUrlLoader : public webapps::WebAppUrlLoader {
 public:
  TestWebAppUrlLoader();
  TestWebAppUrlLoader(const TestWebAppUrlLoader&) = delete;
  TestWebAppUrlLoader& operator=(const TestWebAppUrlLoader&) = delete;
  ~TestWebAppUrlLoader() override;

  // Changes TestWebAppUrlLoader to save LoadUrl() calls. Use
  // ProcessLoadUrlRequests() to process these requests. Useful when
  // trying to delay loading the URL.
  void SaveLoadUrlRequests();

  // Processes all saved LoadUrl calls. The call are processed in the same
  // order they were issued.
  void ProcessLoadUrlRequests();

  // Sets the result for the next loader that will be created. Will load with
  // the result just once.
  void SetNextLoadUrlResult(const GURL& url, Result result);
  // Sets sequential results for next multiple loads of the given |url|.
  void AddNextLoadUrlResults(const GURL& url,
                             const std::vector<Result>& results);

  // `WebAppUrlLoader`:
  void LoadUrl(content::NavigationController::LoadURLParams load_url_params,
               content::WebContents* web_contents,
               UrlComparison url_comparison,
               ResultCallback callback) override;

  void TrackLoadUrlCalls(
      base::RepeatingCallback<void(const GURL& url,
                                   content::WebContents* web_contents,
                                   UrlComparison url_comparison)>

          load_url_tracker) {
    load_url_tracker_ = std::move(load_url_tracker);
  }

 private:
  bool should_save_requests_ = false;

  struct UrlResponses {
    UrlResponses();
    ~UrlResponses();

    // Each LoadUrl() gets next result for a given url.
    base::queue<Result> results;
  };

  std::map<GURL, UrlResponses> next_result_map_;

  std::queue<std::pair<GURL, ResultCallback>> pending_requests_;

  base::RepeatingCallback<void(const GURL& url,
                               content::WebContents* web_contents,
                               UrlComparison url_comparison)>
      load_url_tracker_ = base::DoNothing();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_URL_LOADER_H_

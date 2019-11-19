// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_URL_LOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_URL_LOADER_H_

#include <queue>

#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "url/gurl.h"

namespace web_app {

class TestWebAppUrlLoader : public WebAppUrlLoader {
 public:
  TestWebAppUrlLoader();
  ~TestWebAppUrlLoader() override;

  // Changes TestWebAppUrlLoader to save LoadUrl() calls. Use
  // ProcessLoadUrlRequests() to process these requests. Useful when
  // trying to delay loading the URL.
  void SaveLoadUrlRequests();

  // Processes all saved LoadUrl calls. The call are processed in the same
  // order they were issued.
  void ProcessLoadUrlRequests();

  // Sets the result for the next loader that will be created.
  void SetNextLoadUrlResult(const GURL& url, Result result);

  // WebAppUrlLoader
  void LoadUrl(const GURL& url,
               content::WebContents* web_contents,
               UrlComparison url_comparison,
               ResultCallback callback) override;

 private:
  bool should_save_requests_ = false;

  std::map<GURL, Result> next_result_map_;

  std::queue<std::pair<GURL, ResultCallback>> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppUrlLoader);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_URL_LOADER_H_

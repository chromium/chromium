// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_DATA_RETRIEVER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"

struct WebApplicationInfo;

namespace web_app {

// All WebAppDataRetriever operations are async, so this class posts tasks
// when running callbacks to simulate async behavior in tests as well.
class TestDataRetriever : public WebAppDataRetriever {
 public:
  explicit TestDataRetriever(std::unique_ptr<WebApplicationInfo> web_app_info);
  ~TestDataRetriever() override;

  void GetWebApplicationInfo(content::WebContents* web_contents,
                             GetWebApplicationInfoCallback callback) override;

  void GetIcons(const GURL& app_url,
                const std::vector<GURL>& icon_urls,
                GetIconsCallback callback) override;

 private:
  std::unique_ptr<WebApplicationInfo> web_app_info_;

  DISALLOW_COPY_AND_ASSIGN(TestDataRetriever);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_DATA_RETRIEVER_H_

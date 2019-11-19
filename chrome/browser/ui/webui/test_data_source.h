// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TEST_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_TEST_DATA_SOURCE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

// Serves files at chrome://test/ from //src/chrome/test/data/<root>.
class TestDataSource : public content::URLDataSource {
 public:
  explicit TestDataSource(std::string root);
  ~TestDataSource() override = default;

 private:
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;

  std::string GetMimeType(const std::string& path) override;

  bool ShouldServeMimeTypeAsContentTypeHeader() override;

  bool AllowCaching() override;

  std::string GetSource() override;

  std::string GetContentSecurityPolicyScriptSrc() override;

  GURL GetURLForPath(const std::string& path);

  void ReadFile(const std::string& path,
                const content::URLDataSource::GotDataCallback& callback);

  base::FilePath src_root_;
  base::FilePath gen_root_;

  DISALLOW_COPY_AND_ASSIGN(TestDataSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TEST_DATA_SOURCE_H_

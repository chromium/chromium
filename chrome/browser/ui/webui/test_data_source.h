// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TEST_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_TEST_DATA_SOURCE_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

// Serves files at chrome://test/ from //src/chrome/test/data/<root>.
class TestDataSource : public content::URLDataSource {
 public:
  explicit TestDataSource(std::string root);

  TestDataSource(const TestDataSource&) = delete;
  TestDataSource& operator=(const TestDataSource&) = delete;

  ~TestDataSource() override;

 private:
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

  std::string GetMimeType(const GURL& url) override;

  bool ShouldServeMimeTypeAsContentTypeHeader() override;

  bool AllowCaching() override;

  std::string GetSource() override;

  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

  GURL GetURLForPath(const std::string& path);

  void ReadFile(const std::string& path,
                content::URLDataSource::GotDataCallback callback);

  base::FilePath src_root_;
  base::FilePath gen_root_;
  std::map<std::string, std::string> custom_paths_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TEST_DATA_SOURCE_H_

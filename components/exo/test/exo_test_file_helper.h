// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_FILE_HELPER_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_FILE_HELPER_H_

#include "components/exo/file_helper.h"

namespace exo {

class TestFileHelper : public FileHelper {
 public:
  TestFileHelper();
  TestFileHelper(const TestFileHelper&) = delete;
  TestFileHelper& operator=(const TestFileHelper&) = delete;
  ~TestFileHelper() override;

  // FileHelper:
  std::string GetMimeTypeForUriList() const override;
  bool GetUrlFromPath(const std::string& app_id,
                      const base::FilePath& path,
                      GURL* out) override;
  bool HasUrlsInPickle(const base::Pickle& pickle) override;
  void GetUrlsFromPickle(const std::string& app_id,
                         const base::Pickle& pickle,
                         UrlsFromPickleCallback callback) override;

  void RunUrlsCallback(std::vector<GURL> urls);

 private:
  UrlsFromPickleCallback urls_callback_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_FILE_HELPER_H_

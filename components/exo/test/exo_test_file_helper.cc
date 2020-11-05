// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_file_helper.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "url/gurl.h"

namespace exo {

TestFileHelper::TestFileHelper() = default;

TestFileHelper::~TestFileHelper() = default;

std::string TestFileHelper::GetMimeTypeForUriList() const {
  return "text/uri-list";
}

bool TestFileHelper::GetUrlFromPath(const std::string& app_id,
                                    const base::FilePath& path,
                                    GURL* out) {
  *out = GURL("file://" + path.value());
  return true;
}

bool TestFileHelper::HasUrlsInPickle(const base::Pickle& pickle) {
  return true;
}

void TestFileHelper::GetUrlsFromPickle(const std::string& app_id,
                                       const base::Pickle& pickle,
                                       UrlsFromPickleCallback callback) {
  urls_callback_ = std::move(callback);
}

void TestFileHelper::RunUrlsCallback(std::vector<GURL> urls) {
  std::move(urls_callback_).Run(urls);
}

}  // namespace exo

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/test_files_request_filter.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"

namespace {

bool ShouldHandleTestFileRequestCallback(const std::string& path) {
  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 2 || url_substr[0] != "test")
    return false;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  return base::PathExists(
      test_data_dir.AppendASCII("webui").AppendASCII(url_substr[1]));
}

void HandleTestFileRequestCallback(
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleTestFileRequestCallback(path));
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string contents;
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  CHECK(base::ReadFileToString(
      test_data_dir.AppendASCII("webui").AppendASCII(url_substr[1]),
      &contents));

  base::RefCountedString* ref_contents = new base::RefCountedString();
  ref_contents->as_string() = contents;
  std::move(callback).Run(ref_contents);
}

}  // namespace

namespace test {

content::WebUIDataSource::HandleRequestCallback GetTestFilesRequestFilter() {
  return base::BindRepeating(&HandleTestFileRequestCallback);
}

content::WebUIDataSource::ShouldHandleRequestCallback
GetTestShouldHandleRequest() {
  return base::BindRepeating(&ShouldHandleTestFileRequestCallback);
}

}  // namespace test

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/data_request_filter.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace printing {

namespace {

bool ShouldHandleRequestCallback(const std::string& path) {
  return ParseDataPath(path, nullptr, nullptr);
}

void HandleRequestCallback(const std::string& path,
                           content::WebUIDataSource::GotDataCallback callback) {
  int preview_ui_id;
  int page_index;
  CHECK(ParseDataPath(path, &preview_ui_id, &page_index));

  scoped_refptr<base::RefCountedMemory> data;
  PrintPreviewDataService::GetInstance()->GetDataEntry(preview_ui_id,
                                                       page_index, &data);
  if (data.get()) {
    std::move(callback).Run(data.get());
    return;
  }

  // May be a test request
  if (base::EndsWith(path, "/test.pdf", base::CompareCase::SENSITIVE)) {
    std::string test_pdf_content;
    base::FilePath test_data_path;
    CHECK(base::PathService::Get(base::DIR_TEST_DATA, &test_data_path));
    base::FilePath pdf_path =
        test_data_path.AppendASCII("pdf/test.pdf").NormalizePathSeparators();

    CHECK(base::ReadFileToString(pdf_path, &test_pdf_content));
    scoped_refptr<base::RefCountedString> response =
        base::RefCountedString::TakeString(&test_pdf_content);
    std::move(callback).Run(response.get());
    return;
  }

  // Invalid request.
  auto empty_bytes = base::MakeRefCounted<base::RefCountedBytes>();
  std::move(callback).Run(empty_bytes.get());
}

}  // namespace

void AddDataRequestFilter(content::WebUIDataSource& source) {
  source.SetRequestFilter(base::BindRepeating(&ShouldHandleRequestCallback),
                          base::BindRepeating(&HandleRequestCallback));
}

bool ParseDataPath(const std::string& path, int* ui_id, int* page_index) {
  std::string file_path = path.substr(0, path.find_first_of('?'));
  if (base::EndsWith(file_path, "/test.pdf", base::CompareCase::SENSITIVE))
    return true;

  if (!base::EndsWith(file_path, "/print.pdf", base::CompareCase::SENSITIVE))
    return false;

  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return false;

  int preview_ui_id = -1;
  if (!base::StringToInt(url_substr[0], &preview_ui_id) || preview_ui_id < 0)
    return false;

  int preview_page_index = 0;
  if (!base::StringToInt(url_substr[1], &preview_page_index))
    return false;

  if (ui_id)
    *ui_id = preview_ui_id;
  if (page_index)
    *page_index = preview_page_index;
  return true;
}

}  // namespace printing

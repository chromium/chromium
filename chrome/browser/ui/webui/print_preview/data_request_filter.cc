// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/data_request_filter.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

namespace {

bool ShouldHandleRequestCallback(const std::string& path) {
  return !!ParseDataPath(path);
}

void HandleRequestCallback(const std::string& path,
                           content::WebUIDataSource::GotDataCallback callback) {
  absl::optional<PrintPreviewIdAndPageIndex> parsed = ParseDataPath(path);
  CHECK(parsed);

  scoped_refptr<base::RefCountedMemory> data;
  PrintPreviewDataService::GetInstance()->GetDataEntry(
      parsed->ui_id, parsed->page_index, &data);
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
        base::MakeRefCounted<base::RefCountedString>(
            std::move(test_pdf_content));
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

absl::optional<PrintPreviewIdAndPageIndex> ParseDataPath(
    const std::string& path) {
  PrintPreviewIdAndPageIndex parsed = {
      .ui_id = -1,
      .page_index = 0,
  };

  std::string file_path = path.substr(0, path.find_first_of('?'));
  if (base::EndsWith(file_path, "/test.pdf", base::CompareCase::SENSITIVE))
    return parsed;

  if (!base::EndsWith(file_path, "/print.pdf", base::CompareCase::SENSITIVE))
    return absl::nullopt;

  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return absl::nullopt;

  if (!base::StringToInt(url_substr[0], &parsed.ui_id) || parsed.ui_id < 0)
    return absl::nullopt;

  if (!base::StringToInt(url_substr[1], &parsed.page_index))
    return absl::nullopt;

  return parsed;
}

}  // namespace printing

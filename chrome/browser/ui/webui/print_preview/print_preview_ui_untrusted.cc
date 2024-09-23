// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/ui/webui/print_preview/parse_data_path.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"

namespace printing {

namespace {

bool ShouldHandleRequestCallback(const std::string& path) {
  return !!ParseDataPath(path);
}

}  // namespace

PrintPreviewUIUntrustedConfig::PrintPreviewUIUntrustedConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIPrintHost) {}

PrintPreviewUIUntrustedConfig::~PrintPreviewUIUntrustedConfig() = default;

PrintPreviewUIUntrusted::PrintPreviewUIUntrusted(content::WebUI* web_ui)
    : UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedPrintURL);
  source->SetRequestFilter(base::BindRepeating(&ShouldHandleRequestCallback),
                           base::BindRepeating(&HandleRequestCallback));
}

PrintPreviewUIUntrusted::~PrintPreviewUIUntrusted() = default;

// static
scoped_refptr<base::RefCountedMemory>
PrintPreviewUIUntrusted::GetPrintPreviewData(const std::string& path) {
  std::optional<PrintPreviewIdAndPageIndex> parsed = ParseDataPath(path);
  CHECK(parsed);

  scoped_refptr<base::RefCountedMemory> data;
  PrintPreviewDataService::GetInstance()->GetDataEntry(
      parsed->ui_id, parsed->page_index, &data);
  if (data.get()) {
    return data;
  }

  // Return empty data if this is not a valid request.
  // Not a test request or valid data --> invalid.
  if (!base::EndsWith(path, "/test.pdf", base::CompareCase::SENSITIVE)) {
    return base::MakeRefCounted<base::RefCountedStaticMemory>();
  }

  // May be a test request.
  // Blocking is necessary here because this function must determine if the
  // test data directory exists and grab the test data. This should only happen
  // in tests, unless a user intentionally navigates to a "test.pdf"
  // chrome-untrusted://print path, so will never occur during a normal Print
  // Preview.
  base::ScopedAllowBlocking allow_blocking;
  base::FilePath test_data_path;
  // Test request is invalid if there is no test data dir.
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path)) {
    return base::MakeRefCounted<base::RefCountedStaticMemory>();
  }

  base::FilePath pdf_path =
      test_data_path.AppendASCII("pdf/test.pdf").NormalizePathSeparators();
  std::string test_pdf_content;
  // Also invalid if we can't read the test file.
  if (!base::ReadFileToString(pdf_path, &test_pdf_content)) {
    return base::MakeRefCounted<base::RefCountedStaticMemory>();
  }

  // Valid test request --> Return test pdf contents.
  return base::MakeRefCounted<base::RefCountedString>(
      std::move(test_pdf_content));
}

// static
void PrintPreviewUIUntrusted::HandleRequestCallback(
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  scoped_refptr<base::RefCountedMemory> data =
      PrintPreviewUIUntrusted::GetPrintPreviewData(path);
  std::move(callback).Run(data.get());
}

// static
scoped_refptr<base::RefCountedMemory>
PrintPreviewUIUntrusted::GetPrintPreviewDataForTest(const std::string& path) {
  return GetPrintPreviewData(path);
}

}  // namespace printing

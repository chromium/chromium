// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "printing/print_job_constants.h"

using content::WebContents;

namespace printing {

namespace {

scoped_refptr<base::RefCountedStaticMemory> CreateTestData() {
  const unsigned char kPreviewData[] =
      "%PDF-1.4123461023561203947516345165913487104781236491654192345192345";
  return base::MakeRefCounted<base::RefCountedStaticMemory>(kPreviewData);
}

}  // namespace

class PrintPreviewUIUntrustedUnitTest : public PrintPreviewTest {
 public:
  PrintPreviewUIUntrustedUnitTest() = default;

  PrintPreviewUIUntrustedUnitTest(const PrintPreviewUIUntrustedUnitTest&) =
      delete;
  PrintPreviewUIUntrustedUnitTest& operator=(
      const PrintPreviewUIUntrustedUnitTest&) = delete;

  ~PrintPreviewUIUntrustedUnitTest() override = default;
};

TEST_F(PrintPreviewUIUntrustedUnitTest, PrintPreviewData) {
  PrintPreviewDataService* data_service =
      PrintPreviewDataService::GetInstance();
  scoped_refptr<base::RefCountedMemory> dummy_data = CreateTestData();
  data_service->SetDataEntry(0, 0, dummy_data.get());

  // Valid request for data in the data service.
  scoped_refptr<base::RefCountedMemory> data =
      PrintPreviewUIUntrusted::GetPrintPreviewDataForTest("0/0/print.pdf");
  ASSERT_TRUE(data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Invalid request
  data = PrintPreviewUIUntrusted::GetPrintPreviewDataForTest("1/0/print.pdf");
  ASSERT_TRUE(data);
  EXPECT_EQ(0u, data->size());

  // Valid request for test.pdf data
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string test_pdf_content;
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath pdf_path =
      test_data_path.AppendASCII("pdf/test.pdf").NormalizePathSeparators();
  ASSERT_TRUE(base::ReadFileToString(pdf_path, &test_pdf_content));
  auto test_data =
      base::MakeRefCounted<base::RefCountedString>(std::move(test_pdf_content));
  data = PrintPreviewUIUntrusted::GetPrintPreviewDataForTest("0/0/test.pdf");
  ASSERT_TRUE(data);
  EXPECT_EQ(test_data->size(), data->size());
  EXPECT_TRUE(data->Equals(test_data));
}
}  // namespace printing

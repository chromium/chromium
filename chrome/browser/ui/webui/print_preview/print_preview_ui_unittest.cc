// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_web_ui.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_job_constants.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace printing {

namespace {

scoped_refptr<base::RefCountedBytes> CreateTestData() {
  const unsigned char kBlob[] =
      "%PDF-1.4123461023561203947516345165913487104781236491654192345192345";
  std::vector<unsigned char> preview_data(std::begin(kBlob), std::end(kBlob));
  return base::MakeRefCounted<base::RefCountedBytes>(preview_data);
}

// A fake that just ignores `BadMessageReceived()` calls.
class FakePrintPreviewHandler : public PrintPreviewHandler {
 public:
  FakePrintPreviewHandler() = default;
  FakePrintPreviewHandler(const FakePrintPreviewHandler&) = delete;
  FakePrintPreviewHandler& operator=(const FakePrintPreviewHandler&) = delete;
  ~FakePrintPreviewHandler() override = default;

  // PrintPreviewHandler:
  void BadMessageReceived() override {}
};

// A fake that uses `FakePrintPreviewHandler` instead of the real one.
class FakePrintPreviewUI : public PrintPreviewUI {
 public:
  explicit FakePrintPreviewUI(content::WebUI* web_ui)
      : PrintPreviewUI(web_ui, std::make_unique<FakePrintPreviewHandler>()) {}
  FakePrintPreviewUI(const FakePrintPreviewUI&) = delete;
  FakePrintPreviewUI& operator=(const FakePrintPreviewUI&) = delete;
  ~FakePrintPreviewUI() override = default;
};

}  // namespace

class PrintPreviewUIUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  PrintPreviewUIUnitTest() = default;

  PrintPreviewUIUnitTest(const PrintPreviewUIUnitTest&) = delete;
  PrintPreviewUIUnitTest& operator=(const PrintPreviewUIUnitTest&) = delete;

  ~PrintPreviewUIUnitTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    test_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());

#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (IsOopPrintingEnabled()) {
      print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
          test_remote_, test_print_backend_, /*sandboxed=*/true);
    }
#endif

    test_web_ui_.set_web_contents(web_contents());
    preview_ui_ = std::make_unique<FakePrintPreviewUI>(&test_web_ui_);
    preview_ui_->SetPreviewUIId();
  }

  void TearDown() override {
    preview_ui_.reset();
    PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PrintPreviewUI* GetPreviewUi() { return preview_ui_.get(); }

 private:
  content::TestWebUI test_web_ui_;
  std::unique_ptr<FakePrintPreviewUI> preview_ui_;
  scoped_refptr<TestPrintBackend> test_print_backend_;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
#endif
};

// Create/Get a preview tab for initiator.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewData) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  scoped_refptr<base::RefCountedMemory> data =
      preview_ui->GetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX);
  EXPECT_FALSE(data);

  scoped_refptr<base::RefCountedBytes> dummy_data = CreateTestData();

  preview_ui->SetPrintPreviewDataForIndexForTest(
      COMPLETE_PREVIEW_DOCUMENT_INDEX, dummy_data.get());
  data =
      preview_ui->GetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX);
  ASSERT_TRUE(data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Clear the preview data.
  preview_ui->ClearAllPreviewDataForTest();

  data =
      preview_ui->GetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX);
  EXPECT_FALSE(data);
}

// Set and get the individual draft pages.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewDraftPages) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  scoped_refptr<base::RefCountedMemory> data =
      preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX);
  EXPECT_FALSE(data);

  scoped_refptr<base::RefCountedBytes> dummy_data = CreateTestData();

  preview_ui->SetPrintPreviewDataForIndexForTest(FIRST_PAGE_INDEX,
                                                 dummy_data.get());
  data = preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX);
  ASSERT_TRUE(data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Set and get the third page data.
  preview_ui->SetPrintPreviewDataForIndexForTest(FIRST_PAGE_INDEX + 2,
                                                 dummy_data.get());
  data = preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX + 2);
  ASSERT_TRUE(data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Get the second page data.
  data = preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX + 1);
  EXPECT_FALSE(data);

  preview_ui->SetPrintPreviewDataForIndexForTest(FIRST_PAGE_INDEX + 1,
                                                 dummy_data.get());
  data = preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX + 1);
  ASSERT_TRUE(data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Clear the preview data.
  preview_ui->ClearAllPreviewDataForTest();
  data = preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX);
  EXPECT_FALSE(data);
}

// Test the browser-side print preview cancellation functionality.
TEST_F(PrintPreviewUIUnitTest, ShouldCancelRequest) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  // Test the initial state.
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(
      preview_ui->GetIDForPrintPreviewUI(), 0));

  const int kFirstRequestId = 1000;
  const int kSecondRequestId = 1001;

  // Test with kFirstRequestId.
  preview_ui->OnPrintPreviewRequest(kFirstRequestId);
  EXPECT_FALSE(PrintPreviewUI::ShouldCancelRequest(
      preview_ui->GetIDForPrintPreviewUI(), kFirstRequestId));
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(
      preview_ui->GetIDForPrintPreviewUI(), kSecondRequestId));

  // Test with kSecondRequestId.
  preview_ui->OnPrintPreviewRequest(kSecondRequestId);
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(
      preview_ui->GetIDForPrintPreviewUI(), kFirstRequestId));
  EXPECT_FALSE(PrintPreviewUI::ShouldCancelRequest(
      preview_ui->GetIDForPrintPreviewUI(), kSecondRequestId));
}

// Ensures that a failure cancels all pending actions.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewFailureCancelsPendingActions) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  constexpr int kRequestId = 1;
  preview_ui->OnPrintPreviewRequest(kRequestId);
  EXPECT_FALSE(
      PrintPreviewUI::ShouldCancelRequest(preview_ui->id_, kRequestId));
  preview_ui->OnPrintPreviewFailed(kRequestId);
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(preview_ui->id_, kRequestId));
}

TEST_F(PrintPreviewUIUnitTest, GetPageToNupConvertIndexWithNoPagesToRender) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  // There are no pages to render, so all calls fail.
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(0));
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(1));
}

TEST_F(PrintPreviewUIUnitTest,
       GetPageToNupConvertIndexWithPartialPagesToRender) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  auto params = mojom::DidStartPreviewParams::New();
  params->page_count = 3;
  params->pages_to_render = {1, 2};
  params->pages_per_sheet = 2;
  params->page_size = gfx::SizeF(100, 200);
  preview_ui->DidStartPreview(std::move(params), /*request_id=*/0);

  // There is no page at index 0 to render, so this call fails.
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(0));

  // The page at index 1 in the original document should be the first page
  // (index 0) in the N-up document, prior to the actual N-up conversion.
  // Similarly, the page at index 2 in the original document should be the
  // second page (index 1) in the N-up document, prior to the actual N-up
  // conversion.
  // The actual N-up conversion will put both pages onto 1 sheet, for a 2-up
  // conversion, but that is outside the scope of this test case.
  EXPECT_EQ(0u, preview_ui->GetPageToNupConvertIndex(1));
  EXPECT_EQ(1u, preview_ui->GetPageToNupConvertIndex(2));

  // There is no page at index 3 to render, so this call fails.
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(3));
}

TEST_F(PrintPreviewUIUnitTest, GetPageToNupConvertIndexWithAllPagesToRender) {
  PrintPreviewUI* preview_ui = GetPreviewUi();
  ASSERT_TRUE(preview_ui);

  auto params = mojom::DidStartPreviewParams::New();
  params->page_count = 3;
  params->pages_to_render = {0, 1, 2};
  params->pages_per_sheet = 2;
  params->page_size = gfx::SizeF(100, 200);
  preview_ui->DidStartPreview(std::move(params), /*request_id=*/0);

  // Since all 3 pages are being rendered, the mapping is an identity transform.
  EXPECT_EQ(0u, preview_ui->GetPageToNupConvertIndex(0));
  EXPECT_EQ(1u, preview_ui->GetPageToNupConvertIndex(1));
  EXPECT_EQ(2u, preview_ui->GetPageToNupConvertIndex(2));
  // There is no page at index 3 to render, so this call fails.
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(3));
}

}  // namespace printing

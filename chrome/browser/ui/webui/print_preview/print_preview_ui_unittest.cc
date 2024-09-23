// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_service.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "printing/print_job_constants.h"

using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace printing {

namespace {

scoped_refptr<base::RefCountedBytes> CreateTestData() {
  const unsigned char kBlob[] =
      "%PDF-1.4123461023561203947516345165913487104781236491654192345192345";
  std::vector<unsigned char> preview_data(std::begin(kBlob), std::end(kBlob));
  return base::MakeRefCounted<base::RefCountedBytes>(preview_data);
}

bool IsShowingWebContentsModalDialog(WebContents* tab) {
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(tab);
  return web_contents_modal_dialog_manager->IsDialogActive();
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

// Hands out `FakePrintPreviewUI` instances instead of real ones.
class TestPrintPreviewUIConfig
    : public content::DefaultWebUIConfig<FakePrintPreviewUI> {
 public:
  TestPrintPreviewUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPrintHost) {}
  TestPrintPreviewUIConfig(const TestPrintPreviewUIConfig&) = delete;
  TestPrintPreviewUIConfig& operator=(const TestPrintPreviewUIConfig&) = delete;
  ~TestPrintPreviewUIConfig() override = default;

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override {
    return true;
  }
  bool ShouldHandleURL(const GURL& url) override { return url.path() == "/"; }
};

}  // namespace

class PrintPreviewUIUnitTest : public PrintPreviewTest {
 public:
  PrintPreviewUIUnitTest() = default;

  PrintPreviewUIUnitTest(const PrintPreviewUIUnitTest&) = delete;
  PrintPreviewUIUnitTest& operator=(const PrintPreviewUIUnitTest&) = delete;

  ~PrintPreviewUIUnitTest() override = default;

 protected:
  void SetUp() override {
    PrintPreviewTest::SetUp();

    chrome::NewTab(browser());
  }

  PrintPreviewUI* StartPrintPreview() {
    WebContents* initiator =
        browser()->tab_strip_model()->GetActiveWebContents();
    if (!initiator) {
      ADD_FAILURE();
      return nullptr;
    }

    EXPECT_FALSE(IsShowingWebContentsModalDialog(initiator));

    auto* controller = PrintPreviewDialogController::GetInstance();
    if (!controller) {
      ADD_FAILURE();
      return nullptr;
    }

    PrintViewManager* print_view_manager =
        PrintViewManager::FromWebContents(initiator);
    print_view_manager->PrintPreviewNow(initiator->GetPrimaryMainFrame(),
                                        /*has_selection=*/false);
    WebContents* preview_dialog =
        controller->GetOrCreatePreviewDialogForTesting(initiator);

    EXPECT_NE(initiator, preview_dialog);
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    EXPECT_TRUE(IsShowingWebContentsModalDialog(initiator));

    PrintPreviewUI* preview_ui =
        preview_dialog->GetWebUI()->GetController()->GetAs<PrintPreviewUI>();
    if (!preview_ui) {
      ADD_FAILURE();
      return nullptr;
    }

    preview_ui->SetPreviewUIId();
    return preview_ui;
  }

 private:
  content::ScopedWebUIConfigRegistration webui_registration_{
      std::make_unique<TestPrintPreviewUIConfig>()};
};

// Create/Get a preview tab for initiator.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewData) {
  PrintPreviewUI* preview_ui = StartPrintPreview();
  ASSERT_TRUE(preview_ui);

  scoped_refptr<base::RefCountedMemory> data;
  preview_ui->GetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX,
                                          &data);
  EXPECT_FALSE(data);

  scoped_refptr<base::RefCountedBytes> dummy_data = CreateTestData();

  preview_ui->SetPrintPreviewDataForIndexForTest(
      COMPLETE_PREVIEW_DOCUMENT_INDEX, dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX,
                                          &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Clear the preview data.
  preview_ui->ClearAllPreviewDataForTest();

  preview_ui->GetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX,
                                          &data);
  EXPECT_FALSE(data);
}

// Set and get the individual draft pages.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewDraftPages) {
  PrintPreviewUI* preview_ui = StartPrintPreview();
  ASSERT_TRUE(preview_ui);

  scoped_refptr<base::RefCountedMemory> data;
  preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX, &data);
  EXPECT_FALSE(data);

  scoped_refptr<base::RefCountedBytes> dummy_data = CreateTestData();

  preview_ui->SetPrintPreviewDataForIndexForTest(FIRST_PAGE_INDEX,
                                                 dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX, &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Set and get the third page data.
  preview_ui->SetPrintPreviewDataForIndexForTest(FIRST_PAGE_INDEX + 2,
                                                 dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX + 2, &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Get the second page data.
  preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX + 1, &data);
  EXPECT_FALSE(data);

  preview_ui->SetPrintPreviewDataForIndexForTest(FIRST_PAGE_INDEX + 1,
                                                 dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX + 1, &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Clear the preview data.
  preview_ui->ClearAllPreviewDataForTest();
  preview_ui->GetPrintPreviewDataForIndex(FIRST_PAGE_INDEX, &data);
  EXPECT_FALSE(data);
}

// Test the browser-side print preview cancellation functionality.
TEST_F(PrintPreviewUIUnitTest, ShouldCancelRequest) {
  PrintPreviewUI* preview_ui = StartPrintPreview();
  ASSERT_TRUE(preview_ui);

  // Test the initial state.
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(
      *preview_ui->GetIDForPrintPreviewUI(), 0));

  const int kFirstRequestId = 1000;
  const int kSecondRequestId = 1001;

  // Test with kFirstRequestId.
  preview_ui->OnPrintPreviewRequest(kFirstRequestId);
  EXPECT_FALSE(PrintPreviewUI::ShouldCancelRequest(
      *preview_ui->GetIDForPrintPreviewUI(), kFirstRequestId));
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(
      *preview_ui->GetIDForPrintPreviewUI(), kSecondRequestId));

  // Test with kSecondRequestId.
  preview_ui->OnPrintPreviewRequest(kSecondRequestId);
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(
      *preview_ui->GetIDForPrintPreviewUI(), kFirstRequestId));
  EXPECT_FALSE(PrintPreviewUI::ShouldCancelRequest(
      *preview_ui->GetIDForPrintPreviewUI(), kSecondRequestId));
}

// Ensures that a failure cancels all pending actions.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewFailureCancelsPendingActions) {
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator);

  auto* controller = PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(controller);

  WebContents* preview_dialog =
      controller->GetOrCreatePreviewDialogForTesting(initiator);

  EXPECT_NE(initiator, preview_dialog);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(initiator));

  PrintPreviewUI* preview_ui =
      preview_dialog->GetWebUI()->GetController()->GetAs<PrintPreviewUI>();
  ASSERT_TRUE(preview_ui);
  preview_ui->SetPreviewUIId();

  constexpr int kRequestId = 1;
  preview_ui->OnPrintPreviewRequest(kRequestId);
  EXPECT_FALSE(
      PrintPreviewUI::ShouldCancelRequest(preview_ui->id_, kRequestId));
  preview_ui->OnPrintPreviewFailed(kRequestId);
  EXPECT_TRUE(PrintPreviewUI::ShouldCancelRequest(preview_ui->id_, kRequestId));
}

TEST_F(PrintPreviewUIUnitTest, GetPageToNupConvertIndexWithNoPagesToRender) {
  PrintPreviewUI* preview_ui = StartPrintPreview();
  ASSERT_TRUE(preview_ui);

  // There are no pages to render, so all calls fail.
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(0));
  EXPECT_EQ(kInvalidPageIndex, preview_ui->GetPageToNupConvertIndex(1));
}

TEST_F(PrintPreviewUIUnitTest,
       GetPageToNupConvertIndexWithPartialPagesToRender) {
  PrintPreviewUI* preview_ui = StartPrintPreview();
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
  PrintPreviewUI* preview_ui = StartPrintPreview();
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

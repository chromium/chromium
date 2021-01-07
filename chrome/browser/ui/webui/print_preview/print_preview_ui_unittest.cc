// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/print_messages.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "printing/print_job_constants.h"

using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace printing {

namespace {

scoped_refptr<base::RefCountedBytes> CreateTestData() {
  const unsigned char blob1[] =
      "%PDF-1.4123461023561203947516345165913487104781236491654192345192345";
  std::vector<unsigned char> preview_data(blob1, blob1 + sizeof(blob1));
  return base::MakeRefCounted<base::RefCountedBytes>(preview_data);
}

bool IsShowingWebContentsModalDialog(WebContents* tab) {
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(tab);
  return web_contents_modal_dialog_manager->IsDialogActive();
}

}  // namespace

class PrintPreviewUIUnitTest : public PrintPreviewTest {
 public:
  PrintPreviewUIUnitTest() {}
  ~PrintPreviewUIUnitTest() override {}

 protected:
  void SetUp() override {
    PrintPreviewTest::SetUp();

    chrome::NewTab(browser());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUIUnitTest);
};

// Create/Get a preview tab for initiator.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewData) {
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator);
  EXPECT_FALSE(IsShowingWebContentsModalDialog(initiator));

  PrintPreviewDialogController* controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(controller);

  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintPreviewNow(initiator->GetMainFrame(), false);
  WebContents* preview_dialog = controller->GetOrCreatePreviewDialog(initiator);

  EXPECT_NE(initiator, preview_dialog);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(initiator));

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_dialog->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui);
  preview_ui->SetPreviewUIId();

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
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator);

  PrintPreviewDialogController* controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(controller);

  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintPreviewNow(initiator->GetMainFrame(), false);
  WebContents* preview_dialog = controller->GetOrCreatePreviewDialog(initiator);

  EXPECT_NE(initiator, preview_dialog);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(initiator));

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_dialog->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui);
  preview_ui->SetPreviewUIId();

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
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator);

  PrintPreviewDialogController* controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(controller);

  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintPreviewNow(initiator->GetMainFrame(), false);
  WebContents* preview_dialog = controller->GetOrCreatePreviewDialog(initiator);

  EXPECT_NE(initiator, preview_dialog);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(IsShowingWebContentsModalDialog(initiator));

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_dialog->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui);
  preview_ui->SetPreviewUIId();

  // Test the initial state.
  EXPECT_TRUE(preview_ui->ShouldCancelRequest(0));

  const int kFirstRequestId = 1000;
  const int kSecondRequestId = 1001;

  // Test with kFirstRequestId.
  preview_ui->OnPrintPreviewRequest(kFirstRequestId);
  EXPECT_FALSE(preview_ui->ShouldCancelRequest(kFirstRequestId));
  EXPECT_TRUE(preview_ui->ShouldCancelRequest(kSecondRequestId));

  // Test with kSecondRequestId.
  preview_ui->OnPrintPreviewRequest(kSecondRequestId);
  EXPECT_TRUE(preview_ui->ShouldCancelRequest(kFirstRequestId));
  EXPECT_FALSE(preview_ui->ShouldCancelRequest(kSecondRequestId));
}

TEST_F(PrintPreviewUIUnitTest, ParseDataPath) {
  EXPECT_FALSE(
      PrintPreviewUI::ParseDataPath("pdf/browser_api.js", nullptr, nullptr));
  EXPECT_TRUE(PrintPreviewUI::ParseDataPath("1/2/print.pdf", nullptr, nullptr));

  int ui_id = -1;
  int page_index = -2;
  EXPECT_TRUE(
      PrintPreviewUI::ParseDataPath("3/4/print.pdf", &ui_id, &page_index));
  EXPECT_EQ(ui_id, 3);
  EXPECT_EQ(page_index, 4);
}

}  // namespace printing

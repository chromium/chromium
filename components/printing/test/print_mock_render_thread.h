// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_TEST_PRINT_MOCK_RENDER_THREAD_H_
#define COMPONENTS_PRINTING_TEST_PRINT_MOCK_RENDER_THREAD_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/test/mock_render_thread.h"
#include "printing/buildflags/buildflags.h"

namespace base {
class DictionaryValue;
}

class MockPrinter;
struct PrintHostMsg_DidStartPreview_Params;
struct PrintHostMsg_DidPreviewPage_Params;
struct PrintHostMsg_DidPrintDocument_Params;
struct PrintHostMsg_PreviewIds;
struct PrintHostMsg_ScriptedPrint_Params;
struct PrintMsg_PrintPages_Params;
struct PrintMsg_Print_Params;

// Extends content::MockRenderThread to know about printing
class PrintMockRenderThread : public content::MockRenderThread {
 public:
  PrintMockRenderThread();
  ~PrintMockRenderThread() override;

  // content::RenderThread overrides.
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_io_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

#if BUILDFLAG(ENABLE_PRINTING)
  // Returns the pseudo-printer instance.
  MockPrinter* printer();

  // Call with |response| set to true if the user wants to print.
  // False if the user decides to cancel.
  void set_print_dialog_user_response(bool response);

  // Cancel print preview when print preview has |page| remaining pages.
  void set_print_preview_cancel_page_number(int page);

  // Get the number of pages to generate for print preview.
  int print_preview_pages_remaining() const;

  // Get a vector of print preview pages.
  const std::vector<std::pair<int, uint32_t>>& print_preview_pages() const;
#endif

 private:
  // Overrides base class implementation to add custom handling for print
  bool OnMessageReceived(const IPC::Message& msg) override;

#if BUILDFLAG(ENABLE_PRINTING)
  // PrintRenderFrameHelper expects default print settings.
  void OnGetDefaultPrintSettings(PrintMsg_Print_Params* setting);

  // PrintRenderFrameHelper expects final print settings from the user.
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       PrintMsg_PrintPages_Params* settings);

  void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  void OnDidPrintDocument(const PrintHostMsg_DidPrintDocument_Params& params,
                          IPC::Message* reply_msg);
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void OnDidStartPreview(const PrintHostMsg_DidStartPreview_Params& params,
                         const PrintHostMsg_PreviewIds& ids);
  void OnDidPreviewPage(const PrintHostMsg_DidPreviewPage_Params& params,
                        const PrintHostMsg_PreviewIds& ids);
  void OnCheckForCancel(const PrintHostMsg_PreviewIds& ids, bool* cancel);
#endif

  // For print preview, PrintRenderFrameHelper will update settings.
  void OnUpdatePrintSettings(int document_cookie,
                             const base::DictionaryValue& job_settings,
                             PrintMsg_PrintPages_Params* params,
                             bool* canceled);

  // A mock printer device used for printing tests.
  std::unique_ptr<MockPrinter> printer_;

  // True to simulate user clicking print. False to cancel.
  bool print_dialog_user_response_ = true;

  // Simulates cancelling print preview if |print_preview_pages_remaining_|
  // equals this.
  int print_preview_cancel_page_number_ = -1;

  // Number of pages to generate for print preview.
  int print_preview_pages_remaining_ = 0;

  // Vector of <page_number, content_data_size> that were previewed.
  std::vector<std::pair<int, uint32_t>> print_preview_pages_;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PrintMockRenderThread);
};

#endif  // COMPONENTS_PRINTING_TEST_PRINT_MOCK_RENDER_THREAD_H_

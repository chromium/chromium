// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/test/print_mock_render_thread.h"

#include <stddef.h>

#include "base/run_loop.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/test/mock_printer.h"
#include "ipc/ipc_sync_message.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/common/print_messages.h"
#endif

PrintMockRenderThread::PrintMockRenderThread()
#if BUILDFLAG(ENABLE_PRINTING)
    : printer_(std::make_unique<MockPrinter>())
#endif
{
}

PrintMockRenderThread::~PrintMockRenderThread() = default;

scoped_refptr<base::SingleThreadTaskRunner>
PrintMockRenderThread::GetIOTaskRunner() {
  return io_task_runner_;
}

void PrintMockRenderThread::set_io_task_runner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  io_task_runner_ = task_runner;
}

bool PrintMockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  if (content::MockRenderThread::OnMessageReceived(msg))
    return true;

  // Gives a chance to handle Mojo interfaces as some messages has been
  // converted to Mojo.
  base::RunLoop().RunUntilIdle();

  // Some messages we do special handling.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintMockRenderThread, msg)
#if BUILDFLAG(ENABLE_PRINTING)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_DidPrintDocument,
                                    OnDidPrintDocument)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidStartPreview, OnDidStartPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPreviewPage, OnDidPreviewPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
#endif
#endif  // BUILDFLAG(ENABLE_PRINTING)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if BUILDFLAG(ENABLE_PRINTING)

void PrintMockRenderThread::OnScriptedPrint(
    const printing::mojom::ScriptedPrintParams& params,
    IPC::Message* reply_msg) {
  printing::mojom::PrintPagesParams settings;
  settings.params = printing::mojom::PrintParams::New();
  if (print_dialog_user_response_) {
    printer_->ScriptedPrint(params.cookie, params.expected_pages_count,
                            params.has_selection, &settings);
  }
  PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, settings);
  Send(reply_msg);
}

void PrintMockRenderThread::OnDidPrintDocument(
    const printing::mojom::DidPrintDocumentParams& params,
    IPC::Message* reply_msg) {
  printer_->PrintPage(params);
  PrintHostMsg_DidPrintDocument::WriteReplyParams(reply_msg, true);
  Send(reply_msg);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintMockRenderThread::OnDidStartPreview(
    const printing::mojom::DidStartPreviewParams& params,
    const printing::mojom::PreviewIds& ids) {
  print_preview_pages_remaining_ = params.page_count;
}

void PrintMockRenderThread::OnDidPreviewPage(
    const printing::mojom::DidPreviewPageParams& params,
    const printing::mojom::PreviewIds& ids) {
  uint32_t page_number = params.page_number;
  DCHECK_NE(page_number, printing::kInvalidPageIndex);
  print_preview_pages_remaining_--;
  print_preview_pages_.emplace_back(
      params.page_number, params.content->metafile_data_region.GetSize());
}

void PrintMockRenderThread::OnCheckForCancel(
    const printing::mojom::PreviewIds& ids,
    bool* cancel) {
  *cancel =
      (print_preview_pages_remaining_ == print_preview_cancel_page_number_);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

MockPrinter* PrintMockRenderThread::printer() {
  return printer_.get();
}

void PrintMockRenderThread::set_print_dialog_user_response(bool response) {
  print_dialog_user_response_ = response;
}

void PrintMockRenderThread::set_print_preview_cancel_page_number(
    uint32_t page) {
  print_preview_cancel_page_number_ = page;
}

uint32_t PrintMockRenderThread::print_preview_pages_remaining() const {
  return print_preview_pages_remaining_;
}

const std::vector<std::pair<uint32_t, uint32_t>>&
PrintMockRenderThread::print_preview_pages() const {
  return print_preview_pages_;
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

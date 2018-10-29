// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager.h"

#include "build/build_config.h"
#include "components/printing/common/print_messages.h"

namespace printing {

PrintManager::PrintManager(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      number_pages_(0),
      cookie_(0) {
}

PrintManager::~PrintManager() {
}

bool PrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetDocumentCookie,
                        OnDidGetDocumentCookie)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintingFailed, OnPrintingFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintManager::OnDidGetPrintedPagesCount(int cookie,
                                             int number_pages) {
  DCHECK_GT(cookie, 0);
  DCHECK_GT(number_pages, 0);
  number_pages_ = number_pages;
}

void PrintManager::OnDidGetDocumentCookie(int cookie) {
  cookie_ = cookie;
}

void PrintManager::OnPrintingFailed(int cookie) {
  if (cookie != cookie_) {
    NOTREACHED();
    return;
  }
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

void PrintManager::PrintingRenderFrameDeleted() {
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

#if defined(OS_ANDROID)
void PrintManager::PdfWritingDone(int page_count) {
  if (!pdf_writing_done_callback_.is_null())
    pdf_writing_done_callback_.Run(page_count);
  // Invalidate the file descriptor so it doesn't get reused.
  file_descriptor_ = base::FileDescriptor(-1, false);
}
#endif

}  // namespace printing

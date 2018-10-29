// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(OS_ANDROID)
#include "base/callback.h"
#include "base/file_descriptor_posix.h"
#endif

namespace printing {

class PrintManager : public content::WebContentsObserver {
 public:
  ~PrintManager() override;

#if defined(OS_ANDROID)
  // TODO(timvolodine): consider introducing PrintManagerAndroid (crbug/500960)
  using PdfWritingDoneCallback = base::Callback<void(int /* page count */)>;

  void PdfWritingDone(int page_count);

  // Sets the file descriptor into which the PDF will be written.
  void set_file_descriptor(const base::FileDescriptor& file_descriptor) {
    file_descriptor_ = file_descriptor;
  }

  // Gets the file descriptor into which the PDF will be written.
  base::FileDescriptor file_descriptor() const { return file_descriptor_; }
#endif

 protected:
  explicit PrintManager(content::WebContents* contents);

  // Terminates or cancels the print job if one was pending.
  void PrintingRenderFrameDeleted();

  // content::WebContentsObserver
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // IPC handlers
  virtual void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void OnPrintingFailed(int cookie);

  int number_pages_;  // Number of pages to print in the print job.
  int cookie_;        // The current document cookie.

#if defined(OS_ANDROID)
  // The file descriptor into which the PDF of the page will be written.
  base::FileDescriptor file_descriptor_;

  // Callback to execute when done writing pdf.
  PdfWritingDoneCallback pdf_writing_done_callback_;
#endif

 private:
  void OnDidGetDocumentCookie(int cookie);

  DISALLOW_COPY_AND_ASSIGN(PrintManager);
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_

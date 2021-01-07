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
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/test/mock_render_thread.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_job_constants.h"

class MockPrinter;

// Extends content::MockRenderThread to know about printing
class PrintMockRenderThread : public content::MockRenderThread {
 public:
  PrintMockRenderThread();
  PrintMockRenderThread(const PrintMockRenderThread&) = delete;
  PrintMockRenderThread& operator=(const PrintMockRenderThread&) = delete;
  ~PrintMockRenderThread() override;

#if BUILDFLAG(ENABLE_PRINTING)
  // Returns the pseudo-printer instance.
  MockPrinter* printer();

  // Cancel print preview when print preview has |page| remaining pages.
  void set_print_preview_cancel_page_number(uint32_t page);

  // Get the number of pages to generate for print preview.
  uint32_t print_preview_pages_remaining() const;

  // Get a vector of print preview pages.
  const std::vector<std::pair<uint32_t, uint32_t>>& print_preview_pages() const;

  // Determines whether to cancel a print preview request.
  bool ShouldCancelRequest() const;
#endif

  MockPrinter* GetPrinter() { return printer_.get(); }

 private:
  // Overrides base class implementation to add custom handling for print
  bool OnMessageReceived(const IPC::Message& msg) override;

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void OnDidStartPreview(const printing::mojom::DidStartPreviewParams& params,
                         const printing::mojom::PreviewIds& ids);
  void OnDidPreviewPage(const printing::mojom::DidPreviewPageParams& params,
                        const printing::mojom::PreviewIds& ids);
#endif

  // A mock printer device used for printing tests.
  std::unique_ptr<MockPrinter> printer_;

  // Simulates cancelling print preview if |print_preview_pages_remaining_|
  // equals this.
  uint32_t print_preview_cancel_page_number_ = printing::kInvalidPageIndex;

  // Number of pages to generate for print preview.
  uint32_t print_preview_pages_remaining_ = 0;

  // Vector of <page_number, content_data_size> that were previewed.
  std::vector<std::pair<uint32_t, uint32_t>> print_preview_pages_;
#endif
};

#endif  // COMPONENTS_PRINTING_TEST_PRINT_MOCK_RENDER_THREAD_H_

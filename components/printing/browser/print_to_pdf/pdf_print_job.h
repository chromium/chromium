// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_JOB_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_JOB_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "components/printing/common/print.mojom-forward.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}

namespace print_to_pdf {

class PdfPrintJob : public content::WebContentsObserver {
 public:
  using PrintToPdfCallback =
      base::OnceCallback<void(PdfPrintResult,
                              scoped_refptr<base::RefCountedMemory>)>;

  PdfPrintJob(const PdfPrintJob&) = delete;
  PdfPrintJob& operator=(const PdfPrintJob&) = delete;

  // Starts a PDF print job for the specified web contents that
  // prints the current document pages specified by `page_ranges` with
  // parameters, specified by `print_pages_params` into a PDF document,
  // returned with `callback`. If `page_ranges` is empty, the entire
  // document is printed.
  //
  // The job will delete itself when complete.
  static void StartJob(
      content::WebContents* contents,
      content::RenderFrameHost* rfh,
      const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>& remote,
      const std::string& page_ranges,
      printing::mojom::PrintPagesParamsPtr print_pages_params,
      PrintToPdfCallback callback);

 private:
  PdfPrintJob(content::WebContents* contents,
              content::RenderFrameHost* rfh,
              PrintToPdfCallback callback);
  ~PdfPrintJob() override;

  // WebContentsObserver overrides:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  void OnDidPrintWithParams(printing::mojom::PrintWithParamsResultPtr result);
  void OnCompositeDocumentToPdfDone(
      printing::mojom::PrintCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  void ReportMemoryRegion(const base::ReadOnlySharedMemoryRegion& region);
  void FailJob(PdfPrintResult result);

  raw_ptr<content::RenderFrameHost> printing_rfh_;
  PrintToPdfCallback print_to_pdf_callback_;

  base::WeakPtrFactory<PdfPrintJob> weak_ptr_factory_{this};
};

}  // namespace print_to_pdf

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_JOB_H_

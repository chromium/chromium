// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_job.h"

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#include "components/printing/common/print.mojom.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/printing_utils.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace print_to_pdf {

PdfPrintJob::PdfPrintJob(content::WebContents* contents,
                         content::RenderFrameHost* rfh,
                         PrintToPdfCallback callback)
    : content::WebContentsObserver(contents),
      printing_rfh_(rfh),
      print_to_pdf_callback_(std::move(callback)) {}

PdfPrintJob::~PdfPrintJob() {
  // The callback is supposed to be consumed at this point confirming
  // that the job result was reported to the job starter.
  DCHECK(!print_to_pdf_callback_);
}

void PdfPrintJob::StartJob(
    content::WebContents* contents,
    content::RenderFrameHost* rfh,
    const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>& remote,
    const std::string& page_ranges,
    printing::mojom::PrintPagesParamsPtr print_pages_params,
    PrintToPdfCallback callback) {
  DCHECK(callback);

  if (!rfh->IsRenderFrameLive()) {
    std::move(callback).Run(PdfPrintResult::kPrintFailure, nullptr);
    return;
  }

  absl::variant<printing::PageRanges, PdfPrintResult> pages =
      TextPageRangesToPageRanges(page_ranges);
  if (absl::holds_alternative<PdfPrintResult>(pages)) {
    std::move(callback).Run(absl::get<PdfPrintResult>(pages), nullptr);
    return;
  }

  print_pages_params->pages = absl::get<printing::PageRanges>(pages);

  // Job is self-owned and will delete itself when complete.
  auto* job = new PdfPrintJob(contents, rfh, std::move(callback));
  remote->PrintWithParams(std::move(print_pages_params),
                          base::BindOnce(&PdfPrintJob::OnDidPrintWithParams,
                                         job->weak_ptr_factory_.GetWeakPtr()));
}

void PdfPrintJob::OnDidPrintWithParams(
    printing::mojom::PrintWithParamsResultPtr result) {
  if (result->is_failure_reason()) {
    switch (result->get_failure_reason()) {
      case printing::mojom::PrintFailureReason::kGeneralFailure:
        FailJob(PdfPrintResult::kPrintFailure);
        return;
      case printing::mojom::PrintFailureReason::kInvalidPageRange:
        FailJob(PdfPrintResult::kPageCountExceeded);
        return;
      case printing::mojom::PrintFailureReason::kPrintingInProgress:
        FailJob(PdfPrintResult::kPrintingInProgress);
        return;
    }
  }

  const printing::mojom::DidPrintDocumentParamsPtr& params =
      result->get_data()->params;
  const auto& content = *params->content;
  const auto& region = content.metafile_data_region;
  if (!region.IsValid()) {
    FailJob(PdfPrintResult::kInvalidSharedMemoryRegion);
    return;
  }

  // If the printed data already looks like a PDF, report it now.
  if (printing::LooksLikePdf(region.Map().GetMemoryAsSpan<const uint8_t>())) {
    ReportMemoryRegion(region);
    return;
  }

  // Otherwise assume this is a composite document and invoke compositor.
  printing::PrintCompositeClient::FromWebContents(web_contents())
      ->CompositeDocument(
          params->document_cookie, printing_rfh_, content,
          result->get_data()->accessibility_tree,
          result->get_data()->generate_document_outline,
          printing::mojom::PrintCompositor::DocumentType::kPDF,
          base::BindOnce(&PdfPrintJob::OnCompositeDocumentToPdfDone,
                         weak_ptr_factory_.GetWeakPtr()));
}

void PdfPrintJob::OnCompositeDocumentToPdfDone(
    printing::mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  if (status != printing::mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    FailJob(PdfPrintResult::kPrintFailure);
    return;
  }

  if (!region.IsValid()) {
    FailJob(PdfPrintResult::kInvalidSharedMemoryRegion);
    return;
  }

  ReportMemoryRegion(region);
}

void PdfPrintJob::ReportMemoryRegion(
    const base::ReadOnlySharedMemoryRegion& region) {
  DCHECK(region.IsValid());
  DCHECK(printing::LooksLikePdf(region.Map().GetMemoryAsSpan<const uint8_t>()));

  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid()) {
    FailJob(PdfPrintResult::kInvalidSharedMemoryMapping);
    return;
  }

  std::string data =
      std::string(static_cast<const char*>(mapping.memory()), mapping.size());
  std::move(print_to_pdf_callback_)
      .Run(PdfPrintResult::kPrintSuccess,
           base::MakeRefCounted<base::RefCountedString>(std::move(data)));

  delete this;
}

void PdfPrintJob::FailJob(PdfPrintResult result) {
  DCHECK_NE(result, PdfPrintResult::kPrintSuccess);
  DCHECK(print_to_pdf_callback_);

  std::move(print_to_pdf_callback_).Run(result, nullptr);

  delete this;
}

void PdfPrintJob::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host != printing_rfh_)
    return;

  FailJob(PdfPrintResult::kPrintFailure);
}

}  // namespace print_to_pdf

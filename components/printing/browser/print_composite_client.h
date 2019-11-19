// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_

#include <map>
#include <memory>

#include "base/containers/flat_set.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"

struct PrintHostMsg_DidPrintContent_Params;

namespace printing {

// Class to manage print requests and their communication with pdf
// compositor service.
// Each composite request have a separate interface pointer to connect
// with remote service. The request and its subframe printing results are
// tracked by its document cookie and print page number.
class PrintCompositeClient
    : public content::WebContentsUserData<PrintCompositeClient>,
      public content::WebContentsObserver {
 public:
  explicit PrintCompositeClient(content::WebContents* web_contents);
  ~PrintCompositeClient() override;

  // content::WebContentsObserver
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // IPC message handler.
  void OnDidPrintFrameContent(
      content::RenderFrameHost* render_frame_host,
      int document_cookie,
      const PrintHostMsg_DidPrintContent_Params& params);

  // Instructs the specified subframe to print.
  void PrintCrossProcessSubframe(const gfx::Rect& rect,
                                 int document_cookie,
                                 content::RenderFrameHost* subframe_host);

  // Printing single pages is only used by print preview for early return of
  // rendered results. In this case, the pages share the content with printed
  // document. The document can be collected from the individual pages,
  // avoiding the need to also send the entire document again as a large blob.
  // This is for compositing such a single preview page.
  void DoCompositePageToPdf(
      int cookie,
      content::RenderFrameHost* render_frame_host,
      const PrintHostMsg_DidPrintContent_Params& content,
      mojom::PdfCompositor::CompositePageToPdfCallback callback);

  // Notifies compositor to collect individual pages into a document
  // when processing the individual pages for preview.
  void DoPrepareForDocumentToPdf(
      int document_cookie,
      mojom::PdfCompositor::PrepareForDocumentToPdfCallback callback);

  // Notifies compositor of the total number of pages being concurrently
  // collected into the document, allowing for completion of the composition
  // when all pages have been received.
  void DoCompleteDocumentToPdf(
      int document_cookie,
      uint32_t pages_count,
      mojom::PdfCompositor::CompleteDocumentToPdfCallback callback);

  // Used for compositing the entire document for print preview or actual
  // printing.
  void DoCompositeDocumentToPdf(
      int cookie,
      content::RenderFrameHost* render_frame_host,
      const PrintHostMsg_DidPrintContent_Params& content,
      mojom::PdfCompositor::CompositeDocumentToPdfCallback callback);

  // Get the concurrent composition status for a document.  Identifies if the
  // full document will be compiled from the individual pages; if not then a
  // separate document object will need to be provided.
  bool GetIsDocumentConcurrentlyComposited(int cookie) const;

  void SetUserAgent(const std::string& user_agent) { user_agent_ = user_agent; }

 private:
  friend class content::WebContentsUserData<PrintCompositeClient>;
  // Callback functions for getting the replies.
  static void OnDidCompositePageToPdf(
      mojom::PdfCompositor::CompositePageToPdfCallback callback,
      mojom::PdfCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  void OnDidCompositeDocumentToPdf(
      int document_cookie,
      mojom::PdfCompositor::CompositeDocumentToPdfCallback callback,
      mojom::PdfCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  static void OnDidPrepareForDocumentToPdf(
      mojom::PdfCompositor::PrepareForDocumentToPdfCallback callback,
      mojom::PdfCompositor::Status status);

  void OnDidCompleteDocumentToPdf(
      int document_cookie,
      mojom::PdfCompositor::CompleteDocumentToPdfCallback callback,
      mojom::PdfCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  // Get the request or create a new one if none exists.
  // Since printed pages always share content with its document, they share the
  // same composite request.
  mojom::PdfCompositor* GetCompositeRequest(int cookie);

  // Remove an existing request from |compositor_map_|.
  void RemoveCompositeRequest(int cookie);

  mojo::Remote<mojom::PdfCompositor> CreateCompositeRequest();

  // Stores the mapping between document cookies and their corresponding
  // requests.
  std::map<int, mojo::Remote<mojom::PdfCompositor>> compositor_map_;

  // Stores the mapping between render frame's global unique id and document
  // cookies that requested such frame.
  std::map<uint64_t, base::flat_set<int>> pending_subframe_cookies_;

  // Stores the mapping between document cookie and all the printed subframes
  // for that document.
  std::map<int, base::flat_set<uint64_t>> printed_subframes_;

  // Stores the set of cookies for documents that are doing concurrently
  // composition using individual pages, so that no separate composite request
  // with full-document blob is required.
  base::flat_set<int> is_doc_concurrently_composited_set_;

  std::string user_agent_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PrintCompositeClient);
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_

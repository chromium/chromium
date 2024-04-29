// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_

#include <map>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/printing/common/print.mojom.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace printing {

// Class to manage print requests and their communication with print compositor
// service. Each composite request have a separate interface pointer to connect
// with remote service. The request and its subframe printing results are
// tracked by its document cookie and print page number.
class PrintCompositeClient
    : public content::WebContentsUserData<PrintCompositeClient>,
      public content::WebContentsObserver {
 public:
  explicit PrintCompositeClient(content::WebContents* web_contents);
  PrintCompositeClient(const PrintCompositeClient&) = delete;
  PrintCompositeClient& operator=(const PrintCompositeClient&) = delete;
  ~PrintCompositeClient() override;

  // content::WebContentsObserver
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  void SetAccessibilityTree(int document_cookie,
                            const ui::AXTreeUpdate& accessibility_tree);

  // Instructs the specified subframe to print.
  void PrintCrossProcessSubframe(const gfx::Rect& rect,
                                 int document_cookie,
                                 content::RenderFrameHost* subframe_host);

  // Printing single pages is only used by print preview for early return of
  // rendered results. In this case, the pages share the content with printed
  // document. The document can be collected from the individual pages,
  // avoiding the need to also send the entire document again as a large blob.
  // This is for compositing such a single preview page.
  void CompositePage(int cookie,
                     content::RenderFrameHost* render_frame_host,
                     const mojom::DidPrintContentParams& content,
                     mojom::PrintCompositor::CompositePageCallback callback);

  // Notifies compositor to collect individual pages into a document
  // when processing the individual pages for preview.  The `document_type`
  // specified determines the format of the document passed back in the
  // `callback` from `FinishDocumentComposition()`.
  void PrepareToCompositeDocument(
      int document_cookie,
      content::RenderFrameHost* render_frame_host,
      mojom::PrintCompositor::DocumentType document_type,
      mojom::PrintCompositor::PrepareToCompositeDocumentCallback callback);

  // Notifies compositor of the total number of pages being concurrently
  // collected into the document, allowing for completion of the composition
  // when all pages have been received.  The format of the provided document
  // is of the `document_type` specified in `PrepareToCompositeDocument()`.
  void FinishDocumentComposition(
      int document_cookie,
      uint32_t pages_count,
      mojom::PrintCompositor::FinishDocumentCompositionCallback callback);

  // Used for compositing the entire document for print preview or actual
  // printing.
  void CompositeDocument(
      int cookie,
      content::RenderFrameHost* render_frame_host,
      const mojom::DidPrintContentParams& content,
      const ui::AXTreeUpdate& accessibility_tree,
      mojom::GenerateDocumentOutline generate_document_outline,
      mojom::PrintCompositor::DocumentType document_type,
      mojom::PrintCompositor::CompositeDocumentCallback callback);

  // Get the concurrent composition status for a document.  Identifies if the
  // full document will be compiled from the individual pages; if not then a
  // separate document object will need to be provided.
  bool GetIsDocumentConcurrentlyComposited(int cookie) const;

  void SetUserAgent(const std::string& user_agent) { user_agent_ = user_agent; }

 private:
  friend class content::WebContentsUserData<PrintCompositeClient>;
  FRIEND_TEST_ALL_PREFIXES(PrintBrowserTest,
                           PrintSubframeContentBeforeCompositeClientCreation);

  // Callback functions for getting the replies.
  static void OnDidCompositePage(
      mojom::PrintCompositor::CompositePageCallback callback,
      mojom::PrintCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  void OnDidCompositeDocument(
      int document_cookie,
      mojom::PrintCompositor::CompositeDocumentCallback callback,
      mojom::PrintCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  static void OnDidPrepareToCompositeDocument(
      mojom::PrintCompositor::PrepareToCompositeDocumentCallback callback,
      mojom::PrintCompositor::Status status);

  void OnDidFinishDocumentComposition(
      int document_cookie,
      mojom::PrintCompositor::FinishDocumentCompositionCallback callback,
      mojom::PrintCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region);

  void OnDidPrintFrameContent(content::GlobalRenderFrameHostId rfh_id,
                              int document_cookie,
                              mojom::DidPrintContentParamsPtr params);

  // Creates a new composite request for a given document |cookie|. Since
  // printed pages always share content with its document, they share the same
  // composite request. Launches the compositor in a separate process.
  // If a composite request already exists, it is removed.
  // Returns the created composite request.
  mojom::PrintCompositor* CreateCompositeRequest(
      int cookie,
      content::RenderFrameHost* initiator_frame,
      mojom::PrintCompositor::DocumentType document_type);

  // Remove the existing composite request.
  void RemoveCompositeRequest(int cookie);

  // Checks if the |document_cookie| is not 0 and matches |document_cookie_|.
  bool IsDocumentCookieValid(int document_cookie) const;

  // Get the composite request of a document. |cookie| must be valid and equal
  // to |document_cookie_|.
  mojom::PrintCompositor* GetCompositeRequest(int cookie) const;

  // Helper method to fetch the PrintRenderFrame remote interface pointer
  // associated with a given subframe.
  const mojo::AssociatedRemote<mojom::PrintRenderFrame>& GetPrintRenderFrame(
      content::RenderFrameHost* rfh);

  // Stores the message pipe endpoint for making remote calls to the compositor.
  mojo::Remote<mojom::PrintCompositor> compositor_;

  // Stores the unique sequential cookie of the document being composited.
  // Holds 0 if no document is being composited.
  int document_cookie_ = 0;

  // Stores whether the document is concurrently compositing using individual
  // pages, so that no separate composite request with full-document blob is
  // required.
  bool is_doc_concurrently_composited_ = false;

  // Stores the the frame that initiated the composite request;
  // Holds nullptr if no document is being composited.
  raw_ptr<content::RenderFrameHost> initiator_frame_ = nullptr;

  // Stores the pending subframes for the composited document.
  base::flat_set<raw_ptr<content::RenderFrameHost, CtnExperimental>>
      pending_subframes_;

  // Stores the printed subframes for the composited document.
  base::flat_set<raw_ptr<content::RenderFrameHost, CtnExperimental>>
      printed_subframes_;

  struct RequestedSubFrame {
    RequestedSubFrame(content::GlobalRenderFrameHostId rfh_id,
                      int document_cookie,
                      mojom::DidPrintContentParamsPtr params,
                      bool is_live);
    ~RequestedSubFrame();
    RequestedSubFrame(const RequestedSubFrame&) = delete;
    RequestedSubFrame& operator=(const RequestedSubFrame&) = delete;

    content::GlobalRenderFrameHostId rfh_id_;
    int document_cookie_;
    mojom::DidPrintContentParamsPtr params_;
    bool is_live_;
  };
  base::flat_set<std::unique_ptr<RequestedSubFrame>> requested_subframes_;

  std::string user_agent_;

  // Stores a PrintRenderFrame associated remote with the RenderFrameHost used
  // to bind it. The PrintRenderFrame is used to transmit mojo interface method
  // calls to the associated receiver.
  std::map<content::RenderFrameHost*,
           mojo::AssociatedRemote<mojom::PrintRenderFrame>>
      print_render_frames_;

  base::WeakPtrFactory<PrintCompositeClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_

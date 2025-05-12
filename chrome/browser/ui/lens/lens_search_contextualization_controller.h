// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_

#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/tabs/public/tab_interface.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

class LensSearchController;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace content_extraction {
struct InnerTextResult;
}  // namespace content_extraction

namespace optimization_guide {
struct AIPageContentResult;
}  // namespace optimization_guide

using GetIsContextualSearchboxCallback =
    lens::mojom::LensSidePanelPageHandler::GetIsContextualSearchboxCallback;

namespace lens {

// Callback type alias for page content bytes retrieved. Multiple pieces and
// types of content may be retrieved and returned in `page_contents`.
// `primary_content_type` is the main type used in the request flow and used to
// determine request params and whether updated requests need to be sent.
// `pdf_page_count` is the number of pages in the document being retrieved, not
// necessarily the number of pages in `bytes`. For example, if the document is a
// PDF, `pdf_page_count` is the number of pages in the PDF, while `bytes` could
// be empty because the PDF is too large.
using PageContentRetrievedCallback =
    base::OnceCallback<void(std::vector<lens::PageContent> page_contents,
                            lens::MimeType primary_content_type,
                            std::optional<uint32_t> pdf_page_count)>;

// Controller responsible for handling contextualization logic for Lens flows.
// This includes grabbing content related to the page and issuing Lens requests
// so searchbox requests are contextualized.
class LensSearchContextualizationController {
 public:
  explicit LensSearchContextualizationController(
      LensSearchController* lens_search_controller);
  virtual ~LensSearchContextualizationController();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. The contextualization flow is not currently
    // active.
    kOff,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

  // Starts the contextualization flow without the overlay being shown to the
  // user. Virtual for testing.
  virtual void StartContextualization(
      lens::LensOverlayInvocationSource invocation_source,
      lens::LensOverlayQueryController* lens_overlay_query_controller);

  // Tries to fetch the underlying page content bytes to use for
  // contextualization. If page content can not be retrieved, the callback will
  // be run with no bytes.
  void GetPageContextualization(PageContentRetrievedCallback callback);

 private:
  // Gets the inner HTML for contextualization if flag enabled. Otherwise skip
  // to MaybeGetInnerText().
  void MaybeGetInnerHtml(std::vector<lens::PageContent> page_contents,
                         content::RenderFrameHost* render_frame_host,
                         PageContentRetrievedCallback callback);

  // Callback for when the inner HTML is retrieved from the underlying page.
  // Calls MaybeGetInnerText().
  void OnInnerHtmlReceived(std::vector<lens::PageContent> page_contents,
                           content::RenderFrameHost* render_frame_host,
                           PageContentRetrievedCallback callback,
                           const std::optional<std::string>& result);

  // Gets the inner text for contextualization if flag enabled. Otherwise skip
  // to MaybeGetAnnotatedPageContent().
  void MaybeGetInnerText(std::vector<lens::PageContent> page_contents,
                         content::RenderFrameHost* render_frame_host,
                         PageContentRetrievedCallback callback);

  // Callback for when the inner text is retrieved from the underlying page.
  // Calls MaybeGetAnnotatedPageContent().
  void OnInnerTextReceived(
      std::vector<lens::PageContent> page_contents,
      content::RenderFrameHost* render_frame_host,
      PageContentRetrievedCallback callback,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Gets the annotated page content for contextualization if flag enabled.
  // Otherwise run the callback with the HTML and/or innerText.
  void MaybeGetAnnotatedPageContent(
      std::vector<lens::PageContent> page_contents,
      content::RenderFrameHost* render_frame_host,
      PageContentRetrievedCallback callback);

  // Callback for when the annotated page content is retrieved. Runs the
  // callback with the HTML, innerText, and/or annotated page content.
  void OnAnnotatedPageContentReceived(
      std::vector<lens::PageContent> page_contents,
      PageContentRetrievedCallback callback,
      std::optional<optimization_guide::AIPageContentResult> apc);

#if BUILDFLAG(ENABLE_PDF)
  // Gets the PDF bytes from the IPC call to the PDF renderer if the PDF
  // feature is enabled. Otherwise run the callback with no bytes.
  void MaybeGetPdfBytes(pdf::PDFDocumentHelper* pdf_helper,
                        PageContentRetrievedCallback callback);

  // Receives the PDF bytes from the IPC call to the PDF renderer and stores
  // them in initialization data. `pdf_page_count` is passed to the partial PDF
  // text fetch to be used to determine when to stop fetching.
  void OnPdfBytesReceived(PageContentRetrievedCallback callback,
                          pdf::mojom::PdfListener::GetPdfBytesStatus status,
                          const std::vector<uint8_t>& bytes,
                          uint32_t pdf_page_count);
#endif  // BUILDFLAG(ENABLE_PDF)

  // The current state of the contextualization flow.
  State state_ = State::kOff;

  // Indicates whether the user is currently on a context eligible page.
  bool is_page_context_eligible_ = true;

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // Must be the last member.
  base::WeakPtrFactory<LensSearchContextualizationController> weak_ptr_factory_{
      this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_TAB_CONTEXTUALIZATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_TAB_CONTEXTUALIZATION_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_runner.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "pdf/buildflags.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace viz {
struct CopyOutputBitmapWithMetadata;
}  // namespace viz

namespace optimization_guide {
class PageContextEligibility;
}  // namespace optimization_guide

namespace lens {

class TabContextualizationController : public content::WebContentsObserver {
 public:
  explicit TabContextualizationController(tabs::TabInterface* tab);
  ~TabContextualizationController() override;

  DECLARE_USER_DATA(TabContextualizationController);
  static TabContextualizationController* From(tabs::TabInterface* tab);

  // Callback type alias for when the page context is retrieved.
  using GetPageContextCallback = base::OnceCallback<void(
      std::unique_ptr<lens::ContextualInputData> page_content_data)>;

  // Callback type alias for when the page context and eligibility are
  // retrieved. The frame metadata will be empty if the page is not eligible.
  using GetApcResultCallback = base::OnceCallback<void(
      bool page_context_eligible,
      optimization_guide::AIPageContentResultOrError apc)>;

  // Callback type alias for when the annotated page content is retrieved.
  using GetAnnotatedPageContentCallback =
      optimization_guide::OnAIPageContentDone;

  // Callback type alias for when the screenshot is captured.
  using CaptureScreenshotCallback = base::OnceCallback<void(const SkBitmap&)>;

  // Triggers initial page context eligibility check on the current page.
  // Equivalent to calling `optimization_guide::IsPageContextEligible()` with
  // empty frame_metadata. Only needed for the Chromnient use case, lower
  // priority to implement.
  void GetInitialPageContextEligibility(GetApcResultCallback callback);

  // Returns whether the page is context eligible based on the latest cached
  // state. If the page context eligibility API has not been loaded, this will
  // return false. Only needed for the Chromnient use case, lower priority to
  // implement.
  bool GetCurrentPageContextEligibility();

  // Gets contextual page content for the tab.
  virtual void GetPageContext(GetPageContextCallback callback);

  // Updates current page eligibility once received.
  void OnEligibilityChecked(bool is_page_context_eligible,
                            optimization_guide::AIPageContentResultOrError apc);

  // Starts the steps needed to update the page context eligibility.
  void UpdatePageContextEligibility(GetApcResultCallback callback);

  // Captures a screenshot of the current tab's viewport, downscales it if
  // necessary, and calls the callback with the screenshot.
  virtual void CaptureScreenshot(
      std::optional<lens::ImageEncodingOptions> image_options,
      CaptureScreenshotCallback callback);

 private:
  // Creates the eligibility API if it has not been created.
  void CreatePageContextEligibilityAPI();

  // Called when the page context eligibility API is loaded.
  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // TabInterface::WillDiscardContentsCallback:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Gets the annotated page content from the page context eligibility API.
  void GetAnnotatedPageContent(GetAnnotatedPageContentCallback callback);

  // Callback for when the annotated page content is received. Checks if the
  // page is eligible and returns the result to the callback.
  void OnAnnotatedPageContentReceived(
      GetApcResultCallback callback,
      optimization_guide::AIPageContentResultOrError result);

  // GetApcResultCallback for when the APC and eligibility are received
  // for the GetPageContext flow. Adds the APC to the contextual input data and
  // returns it to the callback.
  void OnApcAndEligibilityReceivedForGetPageContext(
      GetPageContextCallback callback,
      std::unique_ptr<lens::ContextualInputData> data,
      bool page_context_eligible,
      optimization_guide::AIPageContentResultOrError result);

#if BUILDFLAG(ENABLE_PDF)
  // Callback for when the PDF bytes are received. Triggers the PDF page
  // index fetch and adds the pdf data to the contextual input data.
  void OnPdfBytesReceived(std::unique_ptr<lens::ContextualInputData> data,
                          GetPageContextCallback callback,
                          pdf::mojom::PdfListener::GetPdfBytesStatus status,
                          const std::vector<uint8_t>& bytes,
                          uint32_t page_count);

  // Callback for when the PDF page index is received.
  // Adds the PDF page index to the contextual input data and returns it to the
  // callback.
  void OnPdfPageIndexReceived(std::unique_ptr<lens::ContextualInputData> data,
                              GetPageContextCallback callback,
                              std::optional<uint32_t> page_index);
#endif  // BUILDFLAG(ENABLE_PDF)

  // Downscales the screenshot if it exceeds the max dimension and calls the
  // callback with the downscaled screenshot.
  void DownscaleScreenshotAndContinue(
      base::ScopedClosureRunner decrement_capturer_count_runner,
      std::optional<lens::ImageEncodingOptions> image_options,
      CaptureScreenshotCallback callback,
      const viz::CopyOutputBitmapWithMetadata& result);

  // Called when screenshot is captured. Calls the callback with the supplied
  // contextual input data including the screenshot.
  void AddScreenshotToContextDataAndContinue(
      GetPageContextCallback callback,
      std::unique_ptr<lens::ContextualInputData> data,
      const SkBitmap& screenshot);

  // The page context eligibility API if it has been fetched. Can be nullptr.
  raw_ptr<optimization_guide::PageContextEligibility>
      page_context_eligibility_ = nullptr;

  ::ui::ScopedUnownedUserData<TabContextualizationController>
      scoped_unowned_user_data_;

  const raw_ptr<tabs::TabInterface> tab_;

  base::CallbackListSubscription tab_subscription_;

  // Task runner used to downscale the tab screenshot in the background.
  scoped_refptr<base::TaskRunner> screenshot_task_runner_;

  bool is_page_context_eligible_ = false;

  base::WeakPtrFactory<TabContextualizationController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_TAB_CONTEXTUALIZATION_CONTROLLER_H_

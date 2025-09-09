// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_TAB_CONTEXTUALIZATION_CONTROLLER_H_
#define COMPONENTS_LENS_TAB_CONTEXTUALIZATION_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/lens/contextual_input.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/buildflags.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace optimization_guide {
struct AIPageContentResult;
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
      std::optional<optimization_guide::AIPageContentResult> apc)>;

  using GetAnnotatedPageContentCallback = base::OnceCallback<void(
      std::optional<optimization_guide::AIPageContentResult> result)>;

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
  void OnEligibilityChecked(
      bool is_page_context_eligible,
      std::optional<optimization_guide::AIPageContentResult> apc);

  // Starts the steps needed to update the page context eligibility.
  void UpdatePageContextEligibility(GetApcResultCallback callback);

 private:
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
      std::optional<optimization_guide::AIPageContentResult> result);

  // GetApcResultCallback for when the APC and eligibility are received
  // for the GetPageContext flow. Adds the APC to the contextual input data and
  // returns it to the callback.
  void OnApcAndEligibilityReceivedForGetPageContext(
      GetPageContextCallback callback,
      std::unique_ptr<lens::ContextualInputData> data,
      bool page_context_eligible,
      std::optional<optimization_guide::AIPageContentResult> result);

#if BUILDFLAG(ENABLE_PDF)
  void OnPdfBytesReceived(std::unique_ptr<lens::ContextualInputData> data,
                          GetPageContextCallback callback,
                          pdf::mojom::PdfListener::GetPdfBytesStatus status,
                          const std::vector<uint8_t>& bytes,
                          uint32_t page_count);
#endif  // BUILDFLAG(ENABLE_PDF)

  // Captures the screenshot of the tab and returns it to the callback.
  void CaptureScreenshot(base::OnceCallback<void(const SkBitmap&)> callback);

  // Callback for when the screenshot is captured. Adds the screenshot to the
  // contextual input data and returns it to the callback.
  void OnScreenshotCaptured(GetPageContextCallback callback,
                            std::unique_ptr<lens::ContextualInputData> data,
                            const SkBitmap& screenshot);

  ::ui::ScopedUnownedUserData<TabContextualizationController>
      scoped_unowned_user_data_;

  raw_ptr<tabs::TabInterface> tab_;

  base::CallbackListSubscription tab_subscription_;

  bool is_page_context_eligible_ = false;

  base::WeakPtrFactory<TabContextualizationController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // COMPONENTS_LENS_TAB_CONTEXTUALIZATION_CONTROLLER_H_

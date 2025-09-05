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
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

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

  // Callback type alias for when the page context eligibility is retrieved.
  using GetPageContextEligibilityCallback =
      base::OnceCallback<void(bool page_context_eligible)>;

  using GetAnnotatedPageContentCallback = base::OnceCallback<void(
      std::optional<optimization_guide::AIPageContentResult> result)>;

  // Triggers initial page context eligibility check on the current page.
  // Equivalent to calling `optimization_guide::IsPageContextEligible()` with
  // empty frame_metadata. Only needed for the Chromnient use case, lower
  // priority to implement.
  void GetInitialPageContextEligibility(
      GetPageContextEligibilityCallback callback);

  // Returns whether the page is context eligible based on the latest cached
  // state. If the page context eligibility API has not been loaded, this will
  // return false. Only needed for the Chromnient use case, lower priority to
  // implement.
  bool GetCurrentPageContextEligibility();

  // Gets contextual page content for the tab.
  virtual void GetPageContext(GetPageContextCallback callback);

  // Updates current page eligibility once received.
  void OnEligibilityChecked(bool is_page_context_eligible);

  // Starts the steps needed to update the page context eligibility.
  void UpdatePageContextEligibility(GetPageContextEligibilityCallback callback);

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // TabInterface::WillDiscardContentsCallback:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  void GetAnnotatedPageContent(GetAnnotatedPageContentCallback callback);

  // Retrieves annotated page contents.
  void OnAnnotatedPageContentReceived(
      GetPageContextEligibilityCallback callback,
      std::optional<optimization_guide::AIPageContentResult> result);

  // Gets whether page context is eligible and returns it to the callback.
  void IsPageContextEligible(
      const GURL& main_frame_url,
      std::vector<optimization_guide::FrameMetadata> frame_metadata,
      GetPageContextEligibilityCallback callback);

  void CaptureScreenshot(base::OnceCallback<void(const SkBitmap&)> callback);

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

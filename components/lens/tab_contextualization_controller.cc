// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/tab_contextualization_controller.h"

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace lens {

DEFINE_USER_DATA(TabContextualizationController);

TabContextualizationController::TabContextualizationController(
    tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this),
      tab_(tab) {
  tab_subscription_ = tab->RegisterWillDiscardContents(
      base::BindRepeating(&TabContextualizationController::WillDiscardContents,
                          weak_ptr_factory_.GetWeakPtr()));
}

TabContextualizationController::~TabContextualizationController() = default;

void TabContextualizationController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

TabContextualizationController* TabContextualizationController::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void TabContextualizationController::PrimaryPageChanged(content::Page& page) {
  is_page_context_eligible_ = false;
}

void TabContextualizationController::OnEligibilityChecked(
    bool is_page_context_eligible) {
  is_page_context_eligible_ = is_page_context_eligible;
}

void TabContextualizationController::UpdatePageContextEligibility(
    GetPageContextEligibilityCallback callback) {
  auto* render_frame_host = tab_->GetContents()->GetPrimaryMainFrame();
  if (!render_frame_host) {
    return;
  }
  blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  ai_page_content_options->max_meta_elements = 20;
  optimization_guide::GetAIPageContent(
      tab_->GetContents(), std::move(ai_page_content_options),
      base::BindOnce(
          &TabContextualizationController::OnAnnotatedPageContentReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabContextualizationController::OnAnnotatedPageContentReceived(
    GetPageContextEligibilityCallback callback,
    std::optional<optimization_guide::AIPageContentResult> result) {
  // The tab URL is used to check if the page is context eligible.
  const auto& tab_url = tab_->GetContents()->GetLastCommittedURL();

  std::vector<optimization_guide::FrameMetadata> frame_metadata_structs;
  if (result) {
    // Convert the page metadata to a C struct defined in the
    // `optimization_guide` component so it can be passed to the shared library.
    frame_metadata_structs =
        optimization_guide::GetFrameMetadataFromPageContent(result.value());
  }

  IsPageContextEligible(tab_url, std::move(frame_metadata_structs),
                        std::move(callback));
}

void TabContextualizationController::IsPageContextEligible(
    const GURL& main_frame_url,
    std::vector<optimization_guide::FrameMetadata> frame_metadata,
    GetPageContextEligibilityCallback callback) {
  std::move(callback).Run(optimization_guide::IsPageContextEligible(
      main_frame_url.host(), main_frame_url.path(), std::move(frame_metadata),
      nullptr));
}

// TODO(crbug.com/439597165): Check tab eligibility
void TabContextualizationController::GetInitialPageContextEligibility(
    GetPageContextEligibilityCallback callback) {}

bool TabContextualizationController::GetCurrentPageContextEligibility() {
  return is_page_context_eligible_;
}

// TODO(crbug.com/439595898): Get contextual page content.
void TabContextualizationController::GetPageContext(
    GetPageContextCallback callback) {}

}  // namespace lens

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internals/webui/notebooks_internals_page_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace notebooks {

NotebooksInternalsPageHandler::NotebooksInternalsPageHandler(
    mojo::PendingReceiver<notebooks_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<notebooks_internals::mojom::Page> page,
    content::BrowserContext* browser_context,
    NotebooksEligibilityService* notebooks_eligibility_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_context_(browser_context),
      notebooks_eligibility_service_(notebooks_eligibility_service) {
  if (notebooks_eligibility_service_) {
    notebooks_eligibility_service_observation_.Observe(
        notebooks_eligibility_service_);
  }
}

NotebooksInternalsPageHandler::~NotebooksInternalsPageHandler() = default;

void NotebooksInternalsPageHandler::GetFeatureFlagState(
    GetFeatureFlagStateCallback callback) {
  auto flags = notebooks_internals::mojom::FeatureFlagState::New();
  flags->notebooks_feature_enabled =
      base::FeatureList::IsEnabled(features::kNotebooks);
  flags->notebook_home_url = GURL(features::kNotebookHomeURL.Get());
  std::move(callback).Run(std::move(flags));
}

void NotebooksInternalsPageHandler::GetProfileEligibility(
    GetProfileEligibilityCallback callback) {
  auto eligibility = notebooks_internals::mojom::ProfileEligibility::New();
  eligibility->user_eligible =
      notebooks_eligibility_service_
          ? notebooks_eligibility_service_->IsEligible()
          : false;
  std::move(callback).Run(std::move(eligibility));
}

void NotebooksInternalsPageHandler::OnNotebooksEligibilityChanged(
    bool eligible) {
  auto eligibility = notebooks_internals::mojom::ProfileEligibility::New();
  eligibility->user_eligible = eligible;
  page_->OnProfileEligibilityChanged(std::move(eligibility));
}

}  // namespace notebooks

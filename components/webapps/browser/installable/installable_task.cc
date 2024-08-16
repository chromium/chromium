// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_task.h"

#include "base/containers/contains.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace webapps {

InstallableTask::InstallableTask(
    content::WebContents* web_contents,
    base::WeakPtr<InstallableManager> installable_manager,
    const InstallableParams& params,
    InstallableCallback callback,
    InstallablePageData& page_data)
    : web_contents_(web_contents->GetWeakPtr()),
      manager_(installable_manager),
      params_(params),
      callback_(std::move(callback)),
      page_data_(page_data) {
  fetcher_ = std::make_unique<InstallableDataFetcher>(web_contents, page_data);
  evaluator_ = std::make_unique<InstallableEvaluator>(
      web_contents, page_data, params_.installable_criteria);
}

InstallableTask::InstallableTask(const InstallableParams params,
                                 InstallablePageData& page_data)
    : params_(params), page_data_(page_data) {}

InstallableTask::~InstallableTask() = default;

void InstallableTask::Start() {
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::RunCallback() {
  if (callback_) {
    InstallableData data = {
        std::move(errors_),
        page_data_->manifest_url(),
        page_data_->GetManifest(),
        page_data_->WebPageMetadata(),
        page_data_->primary_icon_url(),
        page_data_->primary_icon(),
        page_data_->primary_icon_purpose() ==
            blink::mojom::ManifestImageResource_Purpose::MASKABLE,
        page_data_->screenshots(),
        installability_check_passed_,
    };
    std::move(callback_).Run(data);
  }
}

void InstallableTask::ResetWithError(InstallableStatusCode code) {
  // Some callbacks might be already invalidated on certain resets, so we must
  // check for that.
  // Manifest is assumed to be non-null, so we create an empty one here.
  if (callback_) {
    blink::mojom::Manifest manifest;
    mojom::WebPageMetadata metadata;
    std::move(callback_).Run(InstallableData({code}, GURL(), manifest, metadata,
                                             GURL(), nullptr, false,
                                             std::vector<Screenshot>(), false));
  }
}

void InstallableTask::IncrementStateAndWorkOnNextTask() {
  if ((!errors_.empty() && !params_.is_debug_mode) || state_ == kComplete) {
    manager_->OnTaskFinished();
    RunCallback();
    return;
  }

  state_++;
  CHECK(kInactive < state_ && state_ < kMaxState);

  switch (state_) {
    case kCheckEligibility:
      if (params_.check_eligibility) {
        CheckEligibility();
        return;
      }
      break;
    case kFetchWebPageMetadata:
      if (params_.fetch_metadata) {
        fetcher_->FetchWebPageMetadata(base::BindOnce(
            &InstallableTask::OnFetchedData, base::Unretained(this)));
        return;
      }
      break;
    case kFetchManifest:
      fetcher_->FetchManifest(base::BindOnce(&InstallableTask::OnFetchedData,
                                             base::Unretained(this)));
      return;
    case kCheckInstallability:
      CheckInstallability();
      return;
    case kFetchPrimaryIcon:
      if (params_.valid_primary_icon) {
        fetcher_->CheckAndFetchBestPrimaryIcon(
            base::BindOnce(&InstallableTask::OnFetchedData,
                           base::Unretained(this)),
            params_.prefer_maskable_icon, params_.fetch_favicon);
        return;
      }
      break;
    case kFetchScreenshots:
      if (params_.fetch_screenshots) {
        fetcher_->CheckAndFetchScreenshots(base::BindOnce(
            &InstallableTask::OnFetchedData, base::Unretained(this)));
        return;
      }
      break;
  }
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::OnFetchedData(InstallableStatusCode error) {
  if (error != InstallableStatusCode::NO_ERROR_DETECTED) {
    errors_.push_back(error);
  }
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::CheckEligibility() {
  auto errors = evaluator_->CheckEligibility(web_contents_.get());
  if (!errors.empty()) {
    errors_.insert(errors_.end(), errors.begin(), errors.end());
  }
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::CheckInstallability() {
  auto installable_errors = evaluator_->CheckInstallability();
  if (installable_errors.has_value()) {
    for (auto new_error : installable_errors.value()) {
      if (base::Contains(errors_, new_error)) {
        // Don't add duplicated errors.
        continue;
      }
      errors_.push_back(new_error);
    }
  }
  installability_check_passed_ =
      installable_errors.has_value() && installable_errors->empty();
  IncrementStateAndWorkOnNextTask();
}

}  // namespace webapps

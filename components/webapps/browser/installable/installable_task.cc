// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_task.h"

#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace webapps {

InstallableTask::InstallableTask(
    content::WebContents* web_contents,
    content::ServiceWorkerContext* service_worker_context,
    base::WeakPtr<InstallableManager> installable_manager,
    const InstallableParams& params,
    InstallableCallback callback,
    InstallablePageData& page_data)
    : web_contents_(web_contents->GetWeakPtr()),
      manager_(installable_manager),
      params_(params),
      callback_(std::move(callback)),
      page_data_(page_data) {
  fetcher_ = std::make_unique<InstallableDataFetcher>(
      web_contents, service_worker_context, page_data);
  evaluator_ = std::make_unique<InstallableEvaluator>(
      page_data, params_.check_webapp_manifest_display);
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
        page_data_->manifest->url,
        page_data_->GetManifest(),
        *page_data_->web_page_metadata->metadata,
        page_data_->primary_icon->url,
        page_data_->primary_icon->icon.get(),
        page_data_->primary_icon->purpose ==
            blink::mojom::ManifestImageResource_Purpose::MASKABLE,
        page_data_->screenshots,
        valid_manifest_,
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
    case kCheckEligiblity:
      if (params_.check_eligibility) {
        CheckEligiblity();
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
    case kValidManifest:
      if (params_.valid_manifest) {
        CheckManifestValid();
        return;
      }
      break;
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
    case kCheckServiceWorker:
      if (params_.has_worker) {
        fetcher_->CheckServiceWorker(
            base::BindOnce(&InstallableTask::OnFetchedData,
                           base::Unretained(this)),
            base::BindOnce(&InstallableTask::OnWaitingForServiceWorker,
                           base::Unretained(this)),
            params_.wait_for_worker);
        return;
      }
  }
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::OnFetchedData(InstallableStatusCode error) {
  if (error != NO_ERROR_DETECTED && error != MANIFEST_DEPENDENT_TASK_NOT_RUN) {
    errors_.push_back(error);
  }
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::OnWaitingForServiceWorker() {
  // Set the param |wait_for_worker| to false so we only wait once per task.
  params_.wait_for_worker = false;
  // Reset to previous step so that it can resume from Checking SW.
  state_ = kCheckServiceWorker - 1;

  manager_->OnTaskPaused();
}

void InstallableTask::CheckEligiblity() {
  auto errors = evaluator_->CheckEligiblity(web_contents_.get());
  if (!errors.empty()) {
    errors_.insert(errors_.end(), errors.begin(), errors.end());
  }
  IncrementStateAndWorkOnNextTask();
}

void InstallableTask::CheckManifestValid() {
  if (!blink::IsEmptyManifest(page_data_->GetManifest())) {
    auto errors = evaluator_->CheckManifestValid();
    valid_manifest_ = errors.empty();
    errors_.insert(errors_.end(), errors.begin(), errors.end());
  }
  IncrementStateAndWorkOnNextTask();
}

}  // namespace webapps

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_manager.h"

#include <algorithm>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace webapps {

namespace {

void OnDidCompleteGetAllErrors(
    base::OnceCallback<void(std::vector<content::InstallabilityError>
                                installability_errors)> callback,
    const InstallableData& data) {
  std::vector<content::InstallabilityError> installability_errors;
  for (auto error : data.errors) {
    content::InstallabilityError installability_error =
        GetInstallabilityError(error);
    if (!installability_error.error_id.empty())
      installability_errors.push_back(installability_error);
  }

  std::move(callback).Run(std::move(installability_errors));
}

void OnDidCompleteGetPrimaryIcon(
    base::OnceCallback<void(const SkBitmap*)> callback,
    const InstallableData& data) {
  std::move(callback).Run(data.primary_icon.get());
}

}  // namespace

InstallableManager::InstallableManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<InstallableManager>(*web_contents),
      page_data_(std::make_unique<InstallablePageData>()),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // This is null in unit tests.
  if (web_contents) {
    content::StoragePartition* storage_partition =
        web_contents->GetBrowserContext()->GetStoragePartition(
            web_contents->GetSiteInstance());
    DCHECK(storage_partition);
  }
}

InstallableManager::~InstallableManager() {
}

void InstallableManager::GetData(const InstallableParams& params,
                                 InstallableCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback);
  // Return immediately if we're already working on a task. The new task will be
  // looked at once the current task is finished.
  bool was_active = task_queue_.HasCurrent();
  task_queue_.Add(std::make_unique<InstallableTask>(
      GetWebContents(), weak_factory_.GetWeakPtr(), params, std::move(callback),
      *page_data_));
  if (was_active)
    return;

  task_queue_.Current().Start();
}

void InstallableManager::GetAllErrors(
    base::OnceCallback<void(std::vector<content::InstallabilityError>
                                installability_errors)> callback) {
  DCHECK(callback);
  InstallableParams params;
  params.check_eligibility = true;
  params.installable_criteria = InstallableCriteria::kValidManifestWithIcons;
  params.fetch_screenshots = true;
  params.valid_primary_icon = true;
  params.is_debug_mode = true;
  GetData(params,
          base::BindOnce(OnDidCompleteGetAllErrors, std::move(callback)));
}

void InstallableManager::GetPrimaryIcon(
    base::OnceCallback<void(const SkBitmap*)> callback) {
  DCHECK(callback);
  InstallableParams params;
  params.valid_primary_icon = true;
  GetData(params,
          base::BindOnce(OnDidCompleteGetPrimaryIcon, std::move(callback)));
}

void InstallableManager::SetSequencedTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  sequenced_task_runner_ = task_runner;
}

InstallableStatusCode InstallableManager::manifest_error() const {
  return page_data_->manifest_error();
}

InstallableStatusCode InstallableManager::icon_error() const {
  return page_data_->icon_error();
}

GURL InstallableManager::icon_url() const {
  return page_data_->primary_icon_url();
}

const SkBitmap* InstallableManager::icon() const {
  return page_data_->primary_icon();
}

content::WebContents* InstallableManager::GetWebContents() {
  content::WebContents* contents = web_contents();
  if (!contents || contents->IsBeingDestroyed())
    return nullptr;
  return contents;
}

void InstallableManager::Reset(InstallableStatusCode error) {
  DCHECK(error != InstallableStatusCode::NO_ERROR_DETECTED);
  // Prevent any outstanding callbacks to or from this object from being called.
  weak_factory_.InvalidateWeakPtrs();

  task_queue_.ResetWithError(error);

  page_data_->Reset();

  OnResetData();
}

void InstallableManager::OnTaskFinished() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InstallableManager::FinishAndStartNextTask,
                                weak_factory_.GetWeakPtr()));
}

void InstallableManager::FinishAndStartNextTask() {
  task_queue_.Next();

  if (task_queue_.HasCurrent()) {
    task_queue_.Current().Start();
  }
}

void InstallableManager::PrimaryPageChanged(content::Page& page) {
  Reset(InstallableStatusCode::USER_NAVIGATED);
}

void InstallableManager::DidUpdateWebManifestURL(content::RenderFrameHost* rfh,
                                                 const GURL& manifest_url) {
  // A change in the manifest URL invalidates our entire internal state.
  Reset(InstallableStatusCode::MANIFEST_URL_CHANGED);
}

void InstallableManager::WebContentsDestroyed() {
  // This ensures that we do not just hang callbacks on web_contents being
  // destroyed.
  Reset(InstallableStatusCode::RENDERER_EXITING);
  Observe(nullptr);
}

const GURL& InstallableManager::manifest_url() const {
  return page_data_->manifest_url();
}

const blink::mojom::Manifest& InstallableManager::manifest() const {
  return page_data_->GetManifest();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InstallableManager);

}  // namespace webapps

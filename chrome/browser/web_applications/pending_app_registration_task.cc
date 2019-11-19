// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_registration_task.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

PendingAppRegistrationTaskBase::PendingAppRegistrationTaskBase(
    const GURL& launch_url)
    : launch_url_(launch_url) {}

PendingAppRegistrationTaskBase::~PendingAppRegistrationTaskBase() = default;

int PendingAppRegistrationTask::registration_timeout_in_seconds_ = 40;

PendingAppRegistrationTask::PendingAppRegistrationTask(
    const GURL& launch_url,
    WebAppUrlLoader* url_loader,
    content::WebContents* web_contents,
    RegistrationCallback callback)
    : PendingAppRegistrationTaskBase(launch_url),
      url_loader_(url_loader),
      web_contents_(web_contents),
      callback_(std::move(callback)) {
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
          web_contents_->GetSiteInstance());
  DCHECK(storage_partition);

  service_worker_context_ = storage_partition->GetServiceWorkerContext();
  service_worker_context_->AddObserver(this);

  registration_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(registration_timeout_in_seconds_),
      base::BindOnce(&PendingAppRegistrationTask::OnRegistrationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  // Check to see if there is already a service worker for the launch url.
  service_worker_context_->CheckHasServiceWorker(
      launch_url,
      base::BindOnce(&PendingAppRegistrationTask::OnDidCheckHasServiceWorker,
                     weak_ptr_factory_.GetWeakPtr()));
}

PendingAppRegistrationTask::~PendingAppRegistrationTask() {
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void PendingAppRegistrationTask::OnRegistrationCompleted(const GURL& scope) {
  if (!content::ServiceWorkerContext::ScopeMatches(scope, launch_url()))
    return;

  registration_timer_.Stop();
  std::move(callback_).Run(RegistrationResultCode::kSuccess);
}

void PendingAppRegistrationTask::OnDestruct(
    content::ServiceWorkerContext* context) {
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

void PendingAppRegistrationTask::SetTimeoutForTesting(
    int registration_timeout_in_seconds) {
  registration_timeout_in_seconds_ = registration_timeout_in_seconds;
}

void PendingAppRegistrationTask::OnDidCheckHasServiceWorker(
    content::ServiceWorkerCapability capability) {
  if (capability != content::ServiceWorkerCapability::NO_SERVICE_WORKER) {
    registration_timer_.Stop();
    std::move(callback_).Run(RegistrationResultCode::kAlreadyRegistered);
    return;
  }

  // No action is needed when the URL loads.
  // We wait for OnRegistrationCompleted (or registration timeout).
  url_loader_->LoadUrl(launch_url(), web_contents_,
                       WebAppUrlLoader::UrlComparison::kExact,
                       base::DoNothing());
}

void PendingAppRegistrationTask::OnRegistrationTimeout() {
  std::move(callback_).Run(RegistrationResultCode::kTimeout);
}

}  // namespace web_app

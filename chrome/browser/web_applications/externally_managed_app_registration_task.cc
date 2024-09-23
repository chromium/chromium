// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace web_app {

ExternallyManagedAppRegistrationTaskBase::
    ExternallyManagedAppRegistrationTaskBase(
        GURL install_url,
        const base::TimeDelta registration_timeout)
    : install_url_(std::move(install_url)),
      registration_timeout_(registration_timeout) {}

ExternallyManagedAppRegistrationTaskBase::
    ~ExternallyManagedAppRegistrationTaskBase() = default;

ExternallyManagedAppRegistrationTask::ExternallyManagedAppRegistrationTask(
    GURL install_url,
    const base::TimeDelta registration_timeout,
    webapps::WebAppUrlLoader* url_loader,
    content::WebContents* web_contents,
    RegistrationCallback callback)
    : ExternallyManagedAppRegistrationTaskBase(std::move(install_url),
                                               registration_timeout),
      url_loader_(url_loader),
      web_contents_(web_contents),
      callback_(std::move(callback)) {}

void ExternallyManagedAppRegistrationTask::Start() {
  content::StoragePartition* storage_partition =
      web_contents_->GetBrowserContext()->GetStoragePartition(
          web_contents_->GetSiteInstance());
  DCHECK(storage_partition);

  service_worker_context_ = storage_partition->GetServiceWorkerContext();
  service_worker_context_->AddObserver(this);

  registration_timer_.Start(
      FROM_HERE, registration_timeout(),
      base::BindOnce(
          &ExternallyManagedAppRegistrationTask::OnRegistrationTimeout,
          weak_ptr_factory_.GetWeakPtr()));

  CheckHasServiceWorker();
}

ExternallyManagedAppRegistrationTask::~ExternallyManagedAppRegistrationTask() {
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void ExternallyManagedAppRegistrationTask::OnRegistrationCompleted(
    const GURL& scope) {
  if (!callback_) {
    return;
  }
  if (!content::ServiceWorkerContext::ScopeMatches(scope, install_url())) {
    return;
  }

  registration_timer_.Stop();
  std::move(callback_).Run(RegistrationResultCode::kSuccess);
}

void ExternallyManagedAppRegistrationTask::OnDestruct(
    content::ServiceWorkerContext* context) {
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

void ExternallyManagedAppRegistrationTask::CheckHasServiceWorker() {
  // Note: This can call the callback synchronously
  service_worker_context_->CheckHasServiceWorker(
      install_url(),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(install_url())),
      base::BindOnce(
          &ExternallyManagedAppRegistrationTask::OnDidCheckHasServiceWorker,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternallyManagedAppRegistrationTask::OnDidCheckHasServiceWorker(
    content::ServiceWorkerCapability capability) {
  if (!callback_) {
    return;
  }
  if (capability != content::ServiceWorkerCapability::NO_SERVICE_WORKER) {
    registration_timer_.Stop();
    // This is posted as a task because the serviceworker check can be
    // synchronous.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  RegistrationResultCode::kAlreadyRegistered));
    return;
  }

  url_loader_->LoadUrl(install_url(), web_contents_,
                       webapps::WebAppUrlLoader::UrlComparison::kExact,
                       base::DoNothing());
}

void ExternallyManagedAppRegistrationTask::OnRegistrationTimeout() {
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(RegistrationResultCode::kTimeout);
}

}  // namespace web_app

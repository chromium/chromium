// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"

namespace web_app {

ServiceWorkerRegistrationWaiter::ServiceWorkerRegistrationWaiter(
    content::BrowserContext* browser_context,
    const GURL& url)
    : url_(std::move(url)) {
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(browser_context,
                                                          url_);
  DCHECK(storage_partition);

  service_worker_context_ = storage_partition->GetServiceWorkerContext();
  service_worker_context_->AddObserver(this);
}

ServiceWorkerRegistrationWaiter::~ServiceWorkerRegistrationWaiter() {
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void ServiceWorkerRegistrationWaiter::AwaitRegistration() {
  run_loop_.Run();
}

void ServiceWorkerRegistrationWaiter::OnRegistrationCompleted(
    const GURL& pattern) {
  if (content::ServiceWorkerContext::ScopeMatches(pattern, url_))
    run_loop_.Quit();
}

void ServiceWorkerRegistrationWaiter::OnDestruct(
    content::ServiceWorkerContext* context) {
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

}  // namespace web_app

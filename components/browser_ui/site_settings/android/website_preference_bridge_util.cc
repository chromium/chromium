// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/site_settings/android/website_preference_bridge_util.h"

#include "content/public/browser/browsing_data_filter_builder.h"

ClearLocalStorageHelper::ClearLocalStorageHelper(
    content::BrowsingDataRemover* remover,
    base::OnceClosure callback)
    : remover_(remover), callback_(std::move(callback)) {
  remover_->AddObserver(this);
}

ClearLocalStorageHelper::~ClearLocalStorageHelper() {
  remover_->RemoveObserver(this);
}

// static
void ClearLocalStorageHelper::ClearLocalStorage(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    base::OnceClosure callback) {
  // This has to be a raw pointer, since the Observer inside deletes itself when
  // it receives a callback. Otherwise, the observer would get deleted before
  // that happens.
  auto* remover = browser_context->GetBrowsingDataRemover();
  auto* helper = new ClearLocalStorageHelper(remover, std::move(callback));

  // Use |BrowsingDataRemover| to delete local storage data. This is needed
  // because of partitioned storage, which means that there might be multiple
  // storage keys matching one origin. In this case, this deletes data for the
  // origin both in 1P and 3P contexts.
  auto filter = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete,
      content::BrowsingDataFilterBuilder::OriginMatchingMode::
          kOriginInAllContexts);
  filter->AddOrigin(origin);
  remover->RemoveWithFilterAndReply(
      base::Time::Min(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      std::move(filter), helper);
}

void ClearLocalStorageHelper::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  std::move(callback_).Run();
  delete this;
}

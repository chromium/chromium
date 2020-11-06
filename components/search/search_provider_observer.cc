// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search_provider_observer.h"
#include "components/search/search.h"

SearchProviderObserver::SearchProviderObserver(TemplateURLService* service,
                                               base::RepeatingClosure callback)
    : service_(service),
      is_google_(search::DefaultSearchProviderIsGoogle(service_)),
      callback_(std::move(callback)) {
  if (service_) {
    service_observer_.Add(service_);
  }
}

SearchProviderObserver::~SearchProviderObserver() = default;

bool SearchProviderObserver::is_google() {
  return is_google_;
}

void SearchProviderObserver::OnTemplateURLServiceChanged() {
  DCHECK(service_);
  is_google_ = search::DefaultSearchProviderIsGoogle(service_);
  callback_.Run();
}

void SearchProviderObserver::OnTemplateURLServiceShuttingDown() {
  DCHECK(service_);
  service_observer_.Remove(service_);
  service_ = nullptr;
}

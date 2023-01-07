// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_observer.h"

#include "components/site_engagement/content/site_engagement_service.h"

namespace site_engagement {

SiteEngagementObserver::SiteEngagementObserver(SiteEngagementService* service)
    : service_(nullptr) {
  Observe(service);
}

SiteEngagementObserver::SiteEngagementObserver() : service_(nullptr) {}

SiteEngagementObserver::~SiteEngagementObserver() {
  if (service_)
    service_->RemoveObserver(this);
}

SiteEngagementService* SiteEngagementObserver::GetSiteEngagementService()
    const {
  return service_;
}

void SiteEngagementObserver::Observe(SiteEngagementService* service) {
  if (service == service_)
    return;

  if (service_)
    service_->RemoveObserver(this);

  service_ = service;
  if (service_)
    service->AddObserver(this);
}

}  // namespace site_engagement

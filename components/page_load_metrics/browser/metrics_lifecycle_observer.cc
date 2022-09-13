// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/page_load_metrics/browser/metrics_lifecycle_observer.h"

namespace page_load_metrics {

MetricsLifecycleObserver::MetricsLifecycleObserver(
    content::WebContents* web_contents)
    : observer_(page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents)) {
  observer_->AddLifecycleObserver(this);
}

MetricsLifecycleObserver::~MetricsLifecycleObserver() {
  if (observer_) {
    observer_->RemoveLifecycleObserver(this);
    observer_ = nullptr;
  }
}

void MetricsLifecycleObserver::OnGoingAway() {
  observer_ = nullptr;
}

const PageLoadMetricsObserverDelegate*
MetricsLifecycleObserver::GetDelegateForCommittedLoad() {
  return observer_ ? &observer_->GetDelegateForCommittedLoad() : nullptr;
}

}  // namespace page_load_metrics

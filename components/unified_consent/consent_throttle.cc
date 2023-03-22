// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/consent_throttle.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace unified_consent {

namespace {

void FulfillRequestCallback(
    UrlKeyedDataCollectionConsentHelper::State consent_state,
    ConsentThrottle::RequestCallback callback) {
  DCHECK_NE(consent_state,
            UrlKeyedDataCollectionConsentHelper::State::kInitializing);
  std::move(callback).Run(consent_state ==
                          UrlKeyedDataCollectionConsentHelper::State::kEnabled);
}

}  // namespace

ConsentThrottle::ConsentThrottle(
    std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
        consent_helper,
    base::TimeDelta timeout)
    : consent_helper_(std::move(consent_helper)), timeout_(timeout) {
  DCHECK(consent_helper_);
  handle_observation_.Observe(consent_helper_.get());
}

ConsentThrottle::~ConsentThrottle() = default;

void ConsentThrottle::OnUrlKeyedDataCollectionConsentStateChanged(
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK_EQ(consent_helper, consent_helper_.get());

  auto consent_state = consent_helper->GetConsentState();
  if (consent_state ==
      UrlKeyedDataCollectionConsentHelper::State::kInitializing) {
    return;
  }

  for (auto& request_callback : enqueued_request_callbacks_) {
    FulfillRequestCallback(consent_state, std::move(request_callback));
  }

  enqueued_request_callbacks_.clear();
  request_processing_timer_.Stop();
}

void ConsentThrottle::EnqueueRequest(RequestCallback callback) {
  auto consent_state = consent_helper_->GetConsentState();
  if (consent_state !=
      UrlKeyedDataCollectionConsentHelper::State::kInitializing) {
    FulfillRequestCallback(consent_state, std::move(callback));
    return;
  }

  enqueued_request_callbacks_.emplace_back(std::move(callback));
  if (!request_processing_timer_.IsRunning()) {
    request_processing_timer_.Start(
        FROM_HERE, timeout_,
        base::BindOnce(
            &ConsentThrottle::OnTimeoutExpired,
            // Unretained usage here okay, because this object owns the timer.
            base::Unretained(this)));
  }
}

void ConsentThrottle::OnTimeoutExpired() {
  for (auto& request_callback : enqueued_request_callbacks_) {
    CHECK_EQ(consent_helper_->GetConsentState(),
             UrlKeyedDataCollectionConsentHelper::State::kInitializing);
    std::move(request_callback).Run(false);
  }
  enqueued_request_callbacks_.clear();
}

}  // namespace unified_consent

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

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

  request_processing_timer_.Stop();

  // The request callbacks can modify the vector while running. Swap the vector
  // onto the stack to prevent crashing. https://crbug.com/1483454.
  std::vector<RequestCallback> callbacks;
  std::swap(callbacks, enqueued_request_callbacks_);
  for (auto& request_callback : callbacks) {
    FulfillRequestCallback(consent_state, std::move(request_callback));
  }
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
  CHECK_EQ(consent_helper_->GetConsentState(),
           UrlKeyedDataCollectionConsentHelper::State::kInitializing);

  // The request callbacks can modify the vector while running. Swap the vector
  // onto the stack to prevent crashing. https://crbug.com/1483454.
  std::vector<RequestCallback> callbacks;
  std::swap(callbacks, enqueued_request_callbacks_);
  for (auto& request_callback : callbacks) {
    std::move(request_callback).Run(false);
  }
}

}  // namespace unified_consent

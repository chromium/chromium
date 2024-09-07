// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/offliner_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {

OfflinerClient::OfflinerClient(
    std::unique_ptr<Offliner> offliner,
    const Offliner::ProgressCallback& progress_callback)
    : offliner_(std::move(offliner)), progress_callback_(progress_callback) {}

OfflinerClient::~OfflinerClient() = default;

bool OfflinerClient::LoadAndSave(const SavePageRequest& request,
                                 base::TimeDelta timeout,
                                 CompleteCallback complete_callback) {
  if (Active())
    return false;
  if (!offliner_->LoadAndSave(request,
                              base::BindOnce(&OfflinerClient::OfflinerComplete,
                                             base::Unretained(this)),
                              progress_callback_)) {
    return false;
  }
  stopping_ = false;
  active_request_ = std::make_unique<SavePageRequest>(request);
  complete_callback_ = std::move(complete_callback);
  watchdog_timer_.Start(FROM_HERE, timeout, this,
                        &OfflinerClient::HandleWatchdogTimeout);
  return true;
}

void OfflinerClient::Stop(Offliner::RequestStatus status) {
  if (!active_request_ || stopping_) {
    return;
  }
  if (offliner_->Cancel(base::BindOnce(&OfflinerClient::CancelComplete,
                                       base::Unretained(this), status))) {
    stopping_ = true;
  } else {
    Finish(status);
  }
}

void OfflinerClient::HandleWatchdogTimeout() {
  if (!active_request_ || stopping_)
    return;
  // Check if the offliner can finish up now.
  if (offliner_->HandleTimeout(active_request_->request_id()))
    return;
  if (offliner_->Cancel(base::BindOnce(
          &OfflinerClient::CancelComplete, base::Unretained(this),
          Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT))) {
    stopping_ = true;
  } else {
    Finish(Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT);
  }
}

void OfflinerClient::CancelComplete(Offliner::RequestStatus cancel_reason,
                                    const SavePageRequest& request) {
  watchdog_timer_.Stop();
  if (active_request_)
    Finish(cancel_reason);
}

void OfflinerClient::OfflinerComplete(const SavePageRequest& request,
                                      Offliner::RequestStatus status) {
  if (active_request_)
    Finish(status);
  watchdog_timer_.Stop();
}

void OfflinerClient::Finish(Offliner::RequestStatus status) {
  stopping_ = false;
  std::unique_ptr<SavePageRequest> request = std::move(active_request_);
  std::move(complete_callback_).Run(*request, status);
}

}  // namespace offline_pages

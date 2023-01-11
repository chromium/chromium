// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_CLIENT_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "components/offline_pages/core/background/offliner.h"

namespace offline_pages {
class SavePageRequest;

// Provides an interface to the Offliner and implements a timeout.
class OfflinerClient {
 public:
  typedef base::OnceCallback<void(const SavePageRequest&,
                                  Offliner::RequestStatus)>
      CompleteCallback;

  OfflinerClient(std::unique_ptr<Offliner> offliner,
                 const Offliner::ProgressCallback& progress_callback);
  ~OfflinerClient();

  bool Ready() const { return !active_request_; }
  bool Active() const { return active_request_ != nullptr; }

  // Returns the active request, or null if not active.
  const SavePageRequest* ActiveRequest() const { return active_request_.get(); }

  // Begins offlining the request. Calls complete_callback when successfully
  // complete, or when the request is cancelled due to Stop(), or running out of
  // time. If |LoadAndSave| returns false, the operation failed to start, and
  // |complete_callback| will not be called.
  bool LoadAndSave(const SavePageRequest& request,
                   base::TimeDelta timeout,
                   CompleteCallback complete_callback);
  void Stop(Offliner::RequestStatus status);

 public:
  void HandleWatchdogTimeout();

  void CancelComplete(Offliner::RequestStatus cancel_reason,
                      const SavePageRequest& request);
  void OfflinerComplete(const SavePageRequest& request,
                        Offliner::RequestStatus status);

  void Finish(Offliner::RequestStatus status);

  std::unique_ptr<SavePageRequest> active_request_;
  // Whether awaiting the result of |Offliner::Cancel|.
  bool stopping_ = false;

  CompleteCallback complete_callback_;
  std::unique_ptr<Offliner> offliner_;
  const Offliner::ProgressCallback progress_callback_;
  // Timer to watch for pre-render attempts running too long.
  base::OneShotTimer watchdog_timer_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_CLIENT_H_

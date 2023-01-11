// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_STUB_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_STUB_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/offliner.h"

namespace offline_pages {

// Test class stubbing out the functionality of Offliner.
// It is only used for test support.
class OfflinerStub : public Offliner {
 public:
  OfflinerStub();
  ~OfflinerStub() override;

  bool LoadAndSave(const SavePageRequest& request,
                   CompletionCallback completion_callback,
                   const ProgressCallback& progress_callback) override;

  bool Cancel(CancelCallback callback) override;

  void TerminateLoadIfInProgress() override;

  bool HandleTimeout(int64_t request_id) override;

  void disable_loading() { disable_loading_ = true; }

  void enable_callback(bool enable) { enable_callback_ = enable; }

  bool cancel_called() { return cancel_called_; }

  void reset_cancel_called() { cancel_called_ = false; }

  void enable_snapshot_on_last_retry() { snapshot_on_last_retry_ = true; }

  void set_cancel_delay(base::TimeDelta cancel_delay) {
    cancel_delay_ = cancel_delay;
  }

  bool has_pending_request() const { return pending_request_ != nullptr; }

  bool load_and_save_called() const { return load_and_save_called_; }

 private:
  CompletionCallback completion_callback_;
  std::unique_ptr<SavePageRequest> pending_request_;
  bool disable_loading_;
  bool enable_callback_;
  bool cancel_called_;
  bool snapshot_on_last_retry_;
  bool load_and_save_called_ = false;
  base::TimeDelta cancel_delay_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_STUB_H_

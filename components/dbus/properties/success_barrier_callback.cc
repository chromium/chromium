// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/properties/success_barrier_callback.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace {

class BarrierInfo {
 public:
  BarrierInfo(size_t num_calls_left,
              base::OnceCallback<void(bool)> done_callback);
  void Run(bool success);

 private:
  size_t num_calls_left_;
  base::OnceCallback<void(bool)> done_callback_;
};

BarrierInfo::BarrierInfo(size_t num_calls,
                         base::OnceCallback<void(bool)> done_callback)
    : num_calls_left_(num_calls), done_callback_(std::move(done_callback)) {}

void BarrierInfo::Run(bool success) {
  auto run = [&](bool success) {
    if (done_callback_)
      std::move(done_callback_).Run(success);
  };
  if (num_calls_left_ == 0)
    return;
  num_calls_left_--;
  if (!success)
    run(false);
  else if (num_calls_left_ == 0)  // |success| must be true, no need to check.
    run(true);
}

}  // namespace

base::RepeatingCallback<void(bool)> SuccessBarrierCallback(
    size_t num_calls,
    base::OnceCallback<void(bool)> done_callback) {
  DCHECK_GT(num_calls, 0U);

  return base::BindRepeating(
      &BarrierInfo::Run,
      base::Owned(new BarrierInfo(num_calls, std::move(done_callback))));
}

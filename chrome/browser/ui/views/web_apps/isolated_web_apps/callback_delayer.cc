// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/callback_delayer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"

namespace web_app {
namespace {

constexpr int kRefreshRateHz = 60;

}  // namespace

// A timer that can be paused and resumed.
class Stopwatch {
 public:
  Stopwatch() = default;
  ~Stopwatch() = default;

  void Start() {
    if (!timer_) {
      timer_ = std::make_unique<base::ElapsedTimer>();
    }
  }

  void Pause() {
    if (!timer_) {
      return;
    }
    previously_elapsed_ += timer_->Elapsed();
    timer_.reset();
  }

  base::TimeDelta Elapsed() {
    if (timer_) {
      return previously_elapsed_ + timer_->Elapsed();
    }
    return previously_elapsed_;
  }

 private:
  base::TimeDelta previously_elapsed_;
  std::unique_ptr<base::ElapsedTimer> timer_;
};

CallbackDelayer::CallbackDelayer(
    base::TimeDelta duration,
    double progress_pause_percentage,
    base::RepeatingCallback<void(double)> progress_callback)
    : duration_(duration),
      progress_pause_percentage_(progress_pause_percentage),
      progress_callback_(progress_callback),
      stopwatch_(std::make_unique<Stopwatch>()) {}

CallbackDelayer::~CallbackDelayer() = default;

void CallbackDelayer::StartTimer() {
  stopwatch_->Start();
  repeating_timer_.Start(FROM_HERE, base::Seconds(1) / kRefreshRateHz,
                         base::BindRepeating(&CallbackDelayer::OnTimerTick,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void CallbackDelayer::OnTimerTick() {
  int64_t elapsed = stopwatch_->Elapsed().InMillisecondsRoundedUp();
  double progress = elapsed / duration_.InMillisecondsF();
  progress_callback_.Run(progress);
  if (progress >= 1.0) {
    CHECK(!complete_callback_.is_null());
    repeating_timer_.Stop();
    std::move(complete_callback_).Run();
    return;
  }
  if (progress >= progress_pause_percentage_ && complete_callback_.is_null()) {
    stopwatch_->Pause();
    repeating_timer_.Stop();
  }
}

}  // namespace web_app

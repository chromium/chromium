// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_CALLBACK_DELAYER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_CALLBACK_DELAYER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace web_app {

class Stopwatch;

// Buffers the execution of a callback until a minimum amount of time has
// passed, pausing the timer at a configurable percentage if the incoming
// callback hasn't been invoked. A progress update callback will be invoked at
// 60hz while the timer is running.
//
// Usage example:
//  CallbackDelayer delayer(base::Seconds(3), 0.8,
//                          base::BindOnce(&UpdateProgressBar, ...));
//  long_task->Run(delayer.StartDelayingCallback(
//      base::BindOnce(&OnLongTaskComplete, ...)));
class CallbackDelayer {
 public:
  CallbackDelayer(base::TimeDelta duration,
                  double progress_pause_percentage,
                  base::RepeatingCallback<void(double)> progress_callback);
  ~CallbackDelayer();

  // Starts the timer and returns a callback that, when run, will result in
  // |callback| being run after the configured delay has been reached, and
  // will unpause the timer if necessary.
  template <typename... Args>
  base::OnceCallback<void(Args...)> StartDelayingCallback(
      base::OnceCallback<void(Args...)> callback) {
    CHECK(!started_);
    started_ = true;
    StartTimer();
    return base::BindOnce(&CallbackDelayer::BindArguments<Args...>,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  }

 private:
  template <typename... Args>
  void BindArguments(base::OnceCallback<void(Args...)> callback, Args... args) {
    complete_callback_ = base::BindOnce(std::move(callback), args...);
    StartTimer();
  }

  void StartTimer();
  void OnTimerTick();

  base::TimeDelta duration_;
  double progress_pause_percentage_;
  base::RepeatingCallback<void(double)> progress_callback_;

  bool started_ = false;
  std::unique_ptr<Stopwatch> stopwatch_;
  base::OnceClosure complete_callback_;
  base::RepeatingTimer repeating_timer_;
  base::WeakPtrFactory<CallbackDelayer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_CALLBACK_DELAYER_H_

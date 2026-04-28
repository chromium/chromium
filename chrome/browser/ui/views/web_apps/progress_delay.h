// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROGRESS_DELAY_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROGRESS_DELAY_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace web_app {

class ProgressDelay {
 public:
  ProgressDelay(base::TimeDelta delay_time, int total_steps);
  ~ProgressDelay();

  ProgressDelay(const ProgressDelay&) = delete;
  ProgressDelay& operator=(const ProgressDelay&) = delete;

  void Start(
      base::RepeatingCallback<void(std::optional<double>)> progress_callback);

  void DelayComplete();

 private:
  void OnTimerTick();

  const base::TimeDelta delay_time_;
  const int total_steps_;
  base::RepeatingCallback<void(std::optional<double>)> progress_callback_;
  int steps_taken_ = 0;
  base::RepeatingTimer timer_;
  base::WeakPtrFactory<ProgressDelay> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROGRESS_DELAY_H_

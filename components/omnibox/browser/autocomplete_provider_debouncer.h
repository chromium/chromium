// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_DEBOUNCER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_DEBOUNCER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

// Debounces a method call, i.e. throttles its call rate by delaying its
// invocations and cancelling pending ones when new ones are requested.
class AutocompleteProviderDebouncer {
 public:
  AutocompleteProviderDebouncer(bool from_last_run, int delay_ms);

  ~AutocompleteProviderDebouncer();

  // Request |callback| to be invoked after the debouncing delay. If called
  // while a previous request is still pending, the previous request will be
  // cancelled.
  void RequestRun(base::OnceCallback<void()> callback);

  // Cancels any pending request.
  void CancelRequest();

 private:
  void Run();

  // Whether debounce delays are calculated since |time_last_run_| or the last
  // invocation of |RequestRun|.
  bool from_last_run_ = false;

  // The debounce delay.
  int delay_ms_ = 0;

  // Tracks when to next invoke |callback_|.
  base::OneShotTimer timer_;

  // The last time |Run| was invoked.
  base::TimeTicks time_last_run_;

  // The callback to invoke once |timer_| expires.
  base::OnceCallback<void()> callback_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_DEBOUNCER_H_

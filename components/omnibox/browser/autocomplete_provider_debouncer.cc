// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_provider_debouncer.h"

AutocompleteProviderDebouncer::AutocompleteProviderDebouncer(bool from_last_run,
                                                             int delay_ms)
    : from_last_run_(from_last_run), delay_ms_(delay_ms) {}

AutocompleteProviderDebouncer::~AutocompleteProviderDebouncer() = default;

void AutocompleteProviderDebouncer::RequestRun(
    base::OnceCallback<void()> callback) {
  callback_ = std::move(callback);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(delay_ms_));
  if (from_last_run_)
    delay -= base::TimeTicks::Now() - time_last_run_;

  if (delay <= base::TimeDelta())
    Run();
  else {
    timer_.Start(FROM_HERE, delay,
                 base::BindOnce(&AutocompleteProviderDebouncer::Run,
                                base::Unretained(this)));
  }
}

void AutocompleteProviderDebouncer::CancelRequest() {
  timer_.Stop();
}

void AutocompleteProviderDebouncer::Run() {
  time_last_run_ = base::TimeTicks::Now();
  std::move(callback_).Run();
}

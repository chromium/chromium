// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/time_measurements.h"

#include "base/metrics/histogram.h"
#include "components/subresource_filter/core/common/scoped_timers.h"

namespace subresource_filter {

ScopedUmaHistogramThreadTimer::ScopedUmaHistogramThreadTimer(
    const std::string& name)
    : scoped_uma_histogram_thread_timer_(
          impl::ScopedTimerImplFactory<impl::ThreadTicksProvider>::Start(
              impl::ExportMillisecondsToHistogram(base::Histogram::FactoryGet(
                  name,
                  1,
                  10 * 1000,
                  50,
                  base::HistogramBase::kUmaTargetedHistogramFlag)))) {}

ScopedUmaHistogramThreadTimer::~ScopedUmaHistogramThreadTimer() = default;

ScopedUmaHistogramMicroThreadTimer::ScopedUmaHistogramMicroThreadTimer(
    const std::string& name)
    : scoped_uma_histogram_micro_thread_timer_(
          impl::ScopedTimerImplFactory<impl::ThreadTicksProvider>::Start(
              impl::ExportMicrosecondsToHistogram(base::Histogram::FactoryGet(
                  name,
                  1,
                  1000 * 1000,
                  50,
                  base::HistogramBase::kUmaTargetedHistogramFlag)))) {}

ScopedUmaHistogramMicroThreadTimer::~ScopedUmaHistogramMicroThreadTimer() =
    default;

}  // namespace subresource_filter

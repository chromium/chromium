// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_AT_MOST_ONCE_ENUM_UMA_DEFERRER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_AT_MOST_ONCE_ENUM_UMA_DEFERRER_H_

#include <bitset>

#include "base/metrics/histogram_functions.h"

namespace internal {

// Helper functions for AtMostOnceEnumUmaDeferrer
//
// These functions refers implementation details of
// base/metrics/histogram.cc. Keep them synchronized with that.

base::HistogramBase* GetHistogramExactLinear(const char* name,
                                             int exclusive_max);

// For getting an enumerated histogram.
template <typename T>
base::HistogramBase* GetHistogramEnumeration(const char* name) {
  static_assert(std::is_enum<T>::value, "T is not an enum.");
  // This also ensures that an enumeration that doesn't define kMaxValue fails
  // with a semi-useful error ("no member named 'kMaxValue' in ...").
  static_assert(static_cast<uintmax_t>(T::kMaxValue) <=
                    static_cast<uintmax_t>(INT_MAX) - 1,
                "Enumeration's kMaxValue is out of range of INT_MAX!");
  return GetHistogramExactLinear(name, static_cast<int>(T::kMaxValue) + 1);
}

template <typename T>
void UmaHistogramEnumeration(base::HistogramBase* histogram, T sample) {
  static_assert(std::is_enum<T>::value, "T is not an enum.");
  // This also ensures that an enumeration that doesn't define kMaxValue fails
  // with a semi-useful error ("no member named 'kMaxValue' in ...").
  static_assert(static_cast<uintmax_t>(T::kMaxValue) <=
                    static_cast<uintmax_t>(INT_MAX) - 1,
                "Enumeration's kMaxValue is out of range of INT_MAX!");
  DCHECK_LE(static_cast<uintmax_t>(sample),
            static_cast<uintmax_t>(T::kMaxValue));
  histogram->Add(static_cast<int>(sample));
}

}  // namespace internal

// A helper to defer UMA recording for prerendered pages for a UMA.
//
// A UMA has some options for prerendered pages:
//
// - Don't record any metrics.
// - Record metrics if the page is activated and discard them if cancelled.
// - Record metrics unconditionally.
//
// The first and third options are easy. For the second option, we have to defer
// recording metrics to a histogram. If only one sample per navigation is
// recorded, this is done by storing a sample in observer and recording in
// OnComplete. In more gerenal case, systematic deferrers help us and makes code
// simple. AtMostOnceEnumUmaDeferrer provides deferring mechanism with
// assumptions of UseCounterPageLoadMetricsObserver.
//
// This class has two states: deferred (initial) and non deferred.
// In deferred state, RecordOrDefer* methods collect samples into an inner
// bitset. DisableDeferAndFlush changes the state to non deferred and flush the
// collected samples. In non deferred state, RecordOrDefer bypass
// deferring and just record a sample.
//
// This class uses a bitset to ensure:
//
// - To record a value at most once, i.e. already recorded value is ignored.
// - To reduce memory usage.
template <typename T>
class AtMostOnceEnumUmaDeferrer {
 public:
  explicit AtMostOnceEnumUmaDeferrer(const char* histogram_name);

  AtMostOnceEnumUmaDeferrer(const AtMostOnceEnumUmaDeferrer&) = delete;
  AtMostOnceEnumUmaDeferrer& operator=(const AtMostOnceEnumUmaDeferrer&) =
      delete;

  ~AtMostOnceEnumUmaDeferrer();

  // Changes the state to deferred to non deferred and flush deferred samples.
  //
  // It is not ensured that the recorded order for samples before
  // DisableDeferAndFlush and the order of method calls coincides.
  // DisableDeferAndFlush records deferred samples by the order of valued as
  // int.
  void DisableDeferAndFlush();

  // Just record a sample or put it into internal buffer.
  //
  // Already recorded or deferred sample is ignored.
  void RecordOrDefer(T sample);

  // True iff RecordOrDefer is already involked for given sample.
  bool IsRecordedOrDeferred(T sample) const;

  // Provided only for DCHECK.
  std::bitset<static_cast<size_t>(T::kMaxValue) + 1> recorded_or_deferred()
      const {
    return recorded_or_deferred_;
  }

 private:
  raw_ptr<base::HistogramBase> histogram_;
  bool should_defer_ = true;
  std::bitset<static_cast<size_t>(T::kMaxValue) + 1> recorded_or_deferred_;
};

template <typename T>
AtMostOnceEnumUmaDeferrer<T>::AtMostOnceEnumUmaDeferrer(const char* name) {
  histogram_ = internal::GetHistogramEnumeration<T>(name);
}

template <typename T>
AtMostOnceEnumUmaDeferrer<T>::~AtMostOnceEnumUmaDeferrer() = default;

template <typename T>
void AtMostOnceEnumUmaDeferrer<T>::DisableDeferAndFlush() {
  if (!should_defer_) {
    return;
  }

  should_defer_ = false;

  for (int sample_value = 0; sample_value <= static_cast<int>(T::kMaxValue);
       ++sample_value) {
    if (recorded_or_deferred_.test(sample_value)) {
      internal::UmaHistogramEnumeration(histogram_,
                                        static_cast<T>(sample_value));
    }
  }
}

template <typename T>
void AtMostOnceEnumUmaDeferrer<T>::RecordOrDefer(T sample) {
  DCHECK_LE(static_cast<uintmax_t>(sample),
            static_cast<uintmax_t>(T::kMaxValue));

  int sample_value = static_cast<int>(sample);

  if (recorded_or_deferred_.test(sample_value)) {
    return;
  }

  recorded_or_deferred_.set(sample_value);

  if (!should_defer_) {
    internal::UmaHistogramEnumeration(histogram_, sample);
  }
}

template <typename T>
bool AtMostOnceEnumUmaDeferrer<T>::IsRecordedOrDeferred(T sample) const {
  DCHECK_LE(static_cast<uintmax_t>(sample),
            static_cast<uintmax_t>(T::kMaxValue));

  int sample_value = static_cast<int>(sample);
  return recorded_or_deferred_.test(sample_value);
}

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_AT_MOST_ONCE_ENUM_UMA_DEFERRER_H_

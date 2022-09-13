// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a ScopedTimerImpl class which measures its lifetime using
// different time providers, and can report the measurement via user-defined
// callbacks. The class comes with ScopedTimerImplFactory which has convenient
// factory methods returning timer instances.
//
// Although the classes can be used directly, the most common usages are covered
// by the ScopedTimers and ScopedThreadTimers type aliases. See comments to
// these aliases below.
//
// TODO(pkalinnikov): Consider moving this file to "base/metrics/".

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_SCOPED_TIMERS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_SCOPED_TIMERS_H_

#include <type_traits>
#include <utility>

#include "base/time/time.h"

namespace subresource_filter {

namespace impl {

// ScopedTimerImpl is a multi-purpose scoped timer. It measures time delta from
// its construction until its destruction. For example, by putting an instance
// of this class to the beginning of a scope it is possible to measure how long
// the scope is being executed.
//
// The obtained time measurement is reported via ExportFunctor, which takes
// base::TimeDelta as a parameter.
//
// Time is obtained by means of the TimeProvider static interface:
//  * static bool IsSupported();
//    - Idempotently returns whether the system supports such type of provider.
//  * static void WaitUntilInitialized();
//    - Waits until the provider can be used.
//  * static TimeType Now();
//    - Returns the current time of some TimeType, e.g., base::TimeTicks.
//
// The ExportFunctor is invoked exactly once, upon destruction, if both
// |activate| and TimeProvider::IsSupported() are true. Otherwise, the
// ExportFunctor is not called and no time measurements are performed, making
// the overhead of an instance practically zero.
template <typename TimeProvider, typename ExportFunctor>
class ScopedTimerImpl {
 public:
  // Constructs a timer reporting its measurement to |export_functor|. The timer
  // will be activated iff |activate| && TimeProvider::IsSupported().
  explicit ScopedTimerImpl(ExportFunctor&& export_functor, bool activate = true)
      : export_functor_(std::forward<ExportFunctor>(export_functor)),
        activated_(activate && TimeProvider::IsSupported()) {
    if (activated_) {
      TimeProvider::WaitUntilInitialized();
      construction_time_ = TimeProvider::Now();
    }
  }

  // The class is moveable. The |rhs| becomes deactivated when it is moved from,
  // so that no time measurement will be reported upon its destruction.
  ScopedTimerImpl(ScopedTimerImpl&& rhs)
      : export_functor_(std::forward<ExportFunctor>(rhs.export_functor_)),
        construction_time_(std::move(rhs.construction_time_)),
        activated_(rhs.activated_) {
    rhs.activated_ = false;
  }

  // If |this| was activated before assignment, it reports its measurement
  // before stealing the one from |rhs|.
  ScopedTimerImpl& operator=(ScopedTimerImpl&& rhs) {
    if (&rhs != this) {
      this->~ScopedTimerImpl();
      ::new (this) ScopedTimerImpl(std::move(rhs));
    }
    return *this;
  }

  ScopedTimerImpl(const ScopedTimerImpl&) = delete;
  ScopedTimerImpl& operator=(const ScopedTimerImpl&) = delete;

  ~ScopedTimerImpl() {
    if (activated_)
      export_functor_(TimeProvider::Now() - construction_time_);
  }

 private:
  using TimeType =
      typename std::remove_reference<decltype(TimeProvider::Now())>::type;

  ExportFunctor export_functor_;
  TimeType construction_time_;
  bool activated_ = false;
};

// TimeProvider implementations ------------------------------------------------

class TimeTicksProvider {
 public:
  static bool IsSupported() { return true; }
  static void WaitUntilInitialized() {}
  static base::TimeTicks Now() { return base::TimeTicks::Now(); }
};

using ThreadTicksProvider = base::ThreadTicks;

// ScopedTimerImpl factories ---------------------------------------------------

// The class used to produce scoped timers using a certain |TimeProvider|.
template <typename TimeProvider>
class ScopedTimerImplFactory {
 public:
  // Returns a scoped timer which uses |export_functor| to report its
  // measurement. The timer is activated iff TimeProvider::IsSupported().
  template <typename ExportFunctor>
  static ScopedTimerImpl<TimeProvider, ExportFunctor> Start(
      ExportFunctor&& export_functor) {
    return ScopedTimerImpl<TimeProvider, ExportFunctor>(
        std::forward<ExportFunctor>(export_functor));
  }

  // Similar to the Start method above, but the timer is activated iff
  // |condition| && TimeProvider::IsSupported().
  template <typename ExportFunctor>
  static ScopedTimerImpl<TimeProvider, ExportFunctor> StartIf(
      bool condition,
      ExportFunctor&& export_functor) {
    return ScopedTimerImpl<TimeProvider, ExportFunctor>(
        std::forward<ExportFunctor>(export_functor), condition);
  }

  static bool IsSupported() { return TimeProvider::IsSupported(); }
};

}  // namespace impl

// A factory to produce scoped timers that measure time by means of TimeTicks.
//
// Example usage:
//   void foo() {
//     auto scoped_timer = ScopedTimers::Start([](base::TimeDelta duration) {
//       LOG(INFO) << "Duration: " << duration.InMicroseconds() << " us";
//     });
//     ... Some time-consuming code goes here ...
//   }
using ScopedTimers = impl::ScopedTimerImplFactory<impl::TimeTicksProvider>;

// Similar to ScopedTimers, but the produced timers use ThreadTicks.
using ScopedThreadTimers =
    impl::ScopedTimerImplFactory<impl::ThreadTicksProvider>;

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_SCOPED_TIMERS_H_

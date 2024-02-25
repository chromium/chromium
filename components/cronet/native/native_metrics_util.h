// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_NATIVE_METRICS_UTIL_H_
#define COMPONENTS_CRONET_NATIVE_NATIVE_METRICS_UTIL_H_

#include <optional>

#include "base/time/time.h"
#include "components/cronet/native/generated/cronet.idl_impl_struct.h"

namespace cronet {

namespace native_metrics_util {

// Converts timing metrics stored as TimeTicks into the format expected by the
// native layer: a std::optional<Cronet_DateTime> (which may be valueless if
// either |ticks| or |start_ticks| is null) -- this is returned via |out|. An
// out parameter is used because Cronet IDL structs like Cronet_DateTime aren't
// assignable.
//
// By calculating time values using a base (|start_ticks|, |start_time|) pair,
// time values are normalized. This allows time deltas between pairs of events
// to be accurately computed, even if the system clock changed between those
// events, as long as times for both events were calculated using the same
// (|start_ticks|, |start_time|) pair.
//
// Args:
//
// ticks: The ticks value corresponding to the time of the event -- the returned
//        time corresponds to this event.
//
// start_ticks: Ticks measurement at some base time -- the ticks equivalent of
//              start_time. Should be smaller than ticks.
//
// start_time: Time measurement at some base time -- the time equivalent of
//             start_ticks. Must not be null.
//
// out: The output of the function -- the existing pointee object is mutated to
//      either hold the new Cronet_DateTime or nothing (if either |ticks| or
//      |start_ticks| is null).
void ConvertTime(const base::TimeTicks& ticks,
                 const base::TimeTicks& start_ticks,
                 const base::Time& start_time,
                 std::optional<Cronet_DateTime>* out);

}  // namespace native_metrics_util

}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_NATIVE_METRICS_UTIL_H_

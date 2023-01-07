// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_BEGIN_FRAME_ARGS_TEST_H_
#define COMPONENTS_VIZ_TEST_BEGIN_FRAME_ARGS_TEST_H_

#include <stdint.h>

#include <iosfwd>

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace base {
class TickClock;
}

namespace viz {

// Functions for quickly creating BeginFrameArgs
BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number);

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    base::TimeTicks frame_time);

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    int64_t frame_time,
    int64_t deadline,
    int64_t interval);

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    int64_t frame_time,
    int64_t deadline,
    int64_t interval,
    BeginFrameArgs::BeginFrameArgsType type);

// Creates a BeginFrameArgs using the fake Now value stored on the
// OrderSimpleTaskRunner.
BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    const base::TickClock* now_src);

// gtest helpers -- these *must* be in the same namespace as the types they
// operate on.

// Allow "EXPECT_EQ(args1, args2);"
// We don't define operator!= because EXPECT_NE(args1, args2) isn't all that
// sensible.
bool operator==(const BeginFrameArgs& lhs, const BeginFrameArgs& rhs);

// Allow gtest to pretty print begin frame args.
::std::ostream& operator<<(::std::ostream& os, const BeginFrameArgs& args);
void PrintTo(const BeginFrameArgs& args, ::std::ostream* os);

// Allow "EXPECT_EQ(ack1, ack2);"
bool operator==(const BeginFrameAck& lhs, const BeginFrameAck& rhs);

// Allow gtest to pretty print BeginFrameAcks.
::std::ostream& operator<<(::std::ostream& os, const BeginFrameAck& ack);
void PrintTo(const BeginFrameAck& ack, ::std::ostream* os);

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_BEGIN_FRAME_ARGS_TEST_H_

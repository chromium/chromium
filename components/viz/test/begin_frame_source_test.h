// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_BEGIN_FRAME_SOURCE_TEST_H_
#define COMPONENTS_VIZ_TEST_BEGIN_FRAME_SOURCE_TEST_H_

#include "base/trace_event/traced_value.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Macros to help set up expected calls on the MockBeginFrameObserver.
#define EXPECT_BEGIN_FRAME_DROP(obs, source_id, sequence_number, frame_time, \
                                deadline, interval)                          \
  EXPECT_CALL((obs), OnBeginFrame(CreateBeginFrameArgsForTesting(            \
                         BEGINFRAME_FROM_HERE, source_id, sequence_number,   \
                         frame_time, deadline, interval)))                   \
      .Times(1)                                                              \
      .InSequence((obs).sequence)

#define EXPECT_BEGIN_FRAME_USED(obs, source_id, sequence_number, frame_time, \
                                deadline, interval)                          \
  EXPECT_CALL((obs), OnBeginFrame(CreateBeginFrameArgsForTesting(            \
                         BEGINFRAME_FROM_HERE, source_id, sequence_number,   \
                         frame_time, deadline, interval)))                   \
      .InSequence((obs).sequence)                                            \
      .WillOnce(::testing::SaveArg<0>(&((obs).last_begin_frame_args)))

#define EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_id, sequence_number,        \
                                       frame_time, deadline, interval)         \
  EXPECT_CALL(                                                                 \
      (obs), OnBeginFrame(CreateBeginFrameArgsForTesting(                      \
                 BEGINFRAME_FROM_HERE, source_id, sequence_number, frame_time, \
                 deadline, interval, BeginFrameArgs::MISSED)))                 \
      .InSequence((obs).sequence)                                              \
      .WillOnce(::testing::SaveArg<0>(&((obs).last_begin_frame_args)))

#define EXPECT_BEGIN_FRAME_ARGS_DROP(obs, args) \
  EXPECT_CALL((obs), OnBeginFrame(args)).Times(1).InSequence((obs).sequence)

#define EXPECT_BEGIN_FRAME_ARGS_USED(obs, args) \
  EXPECT_CALL((obs), OnBeginFrame(args))        \
      .InSequence((obs).sequence)               \
      .WillOnce(::testing::SaveArg<0>(&((obs).last_begin_frame_args)))

#define EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, paused)         \
  EXPECT_CALL((obs), OnBeginFrameSourcePausedChanged(paused)) \
      .Times(1)                                               \
      .InSequence((obs).sequence)

// Macros to send BeginFrameArgs on a FakeBeginFrameSink (and verify
// resulting observer behaviour).
#define SEND_BEGIN_FRAME(args_equal_to, source, sequence_number, frame_time, \
                         deadline, interval)                                 \
  {                                                                          \
    BeginFrameArgs old_args = (source).TestLastUsedBeginFrameArgs();         \
    BeginFrameArgs new_args = CreateBeginFrameArgsForTesting(                \
        BEGINFRAME_FROM_HERE, (source).source_id(), sequence_number,         \
        frame_time, deadline, interval);                                     \
    ASSERT_FALSE(old_args == new_args);                                      \
    (source).TestOnBeginFrame(new_args);                                     \
    EXPECT_EQ(args_equal_to, (source).TestLastUsedBeginFrameArgs());         \
  }

// When dropping LastUsedBeginFrameArgs **shouldn't** change.
#define SEND_BEGIN_FRAME_DROP(source, sequence_number, frame_time, deadline, \
                              interval)                                      \
  SEND_BEGIN_FRAME(old_args, source, sequence_number, frame_time, deadline,  \
                   interval);

// When used LastUsedBeginFrameArgs **should** be updated.
#define SEND_BEGIN_FRAME_USED(source, sequence_number, frame_time, deadline, \
                              interval)                                      \
  SEND_BEGIN_FRAME(new_args, source, sequence_number, frame_time, deadline,  \
                   interval);

namespace viz {

class MockBeginFrameObserver : public BeginFrameObserver {
 public:
  MOCK_METHOD1(OnBeginFrame, void(const BeginFrameArgs&));
  MOCK_CONST_METHOD0(LastUsedBeginFrameArgs, const BeginFrameArgs&());
  MOCK_METHOD1(OnBeginFrameSourcePausedChanged, void(bool));
  MOCK_CONST_METHOD0(WantsAnimateOnlyBeginFrames, bool());
  MOCK_CONST_METHOD0(IsRoot, bool());

  virtual void AsValueInto(base::trace_event::TracedValue* dict) const;

  // A value different from the normal default returned by a
  // BeginFrameObserver so it is easiable traced back here.
  static const BeginFrameArgs kDefaultBeginFrameArgs;

  MockBeginFrameObserver();
  ~MockBeginFrameObserver() override;

  BeginFrameArgs last_begin_frame_args;
  ::testing::Sequence sequence;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_BEGIN_FRAME_SOURCE_TEST_H_

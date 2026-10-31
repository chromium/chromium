// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/begin_frame_source_test.h"

#include "base/location.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

void MockBeginFrameObserver::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  dict->SetString("type", "MockBeginFrameObserver");
  dict->BeginDictionary("last_begin_frame_args");
  last_begin_frame_args.AsValueInto(dict);
  dict->EndDictionary();
}

// static
BeginFrameArgs MockBeginFrameObserver::GetDefaultBeginFrameArgs() {
  return CreateBeginFrameArgsForTesting(
#ifdef NDEBUG
      nullptr,
#else
      FROM_HERE,
#endif  // NDEBUG
      BeginFrameArgs::kManualSourceId, BeginFrameArgs::kStartingFrameNumber, -1,
      -1, -1);
}

MockBeginFrameObserver::MockBeginFrameObserver()
    : last_begin_frame_args(GetDefaultBeginFrameArgs()) {
  ON_CALL(*this, LastUsedBeginFrameArgs())
      .WillByDefault(::testing::ReturnPointee(&last_begin_frame_args));
  ON_CALL(*this, WantsAnimateOnlyBeginFrames())
      .WillByDefault(::testing::Return(false));
}

MockBeginFrameObserver::~MockBeginFrameObserver() = default;

MockInputClient::MockInputClient() = default;
MockInputClient::~MockInputClient() = default;

}  // namespace viz

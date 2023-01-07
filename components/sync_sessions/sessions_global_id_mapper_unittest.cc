// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_global_id_mapper.h"

#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

const base::Time kTime1 = base::Time::FromInternalValue(110);
const base::Time kTime2 = base::Time::FromInternalValue(120);
const base::Time kTime3 = base::Time::FromInternalValue(130);
const base::Time kTime4 = base::Time::FromInternalValue(140);
const base::Time kTime5 = base::Time::FromInternalValue(150);

// Tests that GetLatestGlobalId returns correct mappings for updated global_ids.
TEST(SessionsGlobalIdMapperTest, GetLatestGlobalId) {
  SessionsGlobalIdMapper mapper;

  mapper.TrackNavigationId(kTime1, /*unique_id=*/1);
  mapper.TrackNavigationId(kTime2, /*unique_id=*/2);
  mapper.TrackNavigationId(kTime3, /*unique_id=*/2);
  mapper.TrackNavigationId(kTime4, /*unique_id=*/2);

  EXPECT_EQ(kTime1.ToInternalValue(),
            mapper.GetLatestGlobalId(kTime1.ToInternalValue()));
  EXPECT_EQ(kTime4.ToInternalValue(),
            mapper.GetLatestGlobalId(kTime2.ToInternalValue()));
  EXPECT_EQ(kTime4.ToInternalValue(),
            mapper.GetLatestGlobalId(kTime3.ToInternalValue()));
  EXPECT_EQ(kTime4.ToInternalValue(),
            mapper.GetLatestGlobalId(kTime4.ToInternalValue()));
  // kTime5 is not mapped, so itself should be returned.
  EXPECT_EQ(kTime5.ToInternalValue(),
            mapper.GetLatestGlobalId(kTime5.ToInternalValue()));
}

// Tests that the global_id mapping is eventually dropped after we reach our
// threshold for the amount to remember.
TEST(SessionsGlobalIdMapperTest, Cleanup) {
  SessionsGlobalIdMapper mapper;

  base::Time current_time = kTime1;
  mapper.TrackNavigationId(current_time, /*unique_id=*/1);

  for (int i = 0; i < 105; i++) {
    current_time =
        base::Time::FromInternalValue(current_time.ToInternalValue() + 1);
    mapper.TrackNavigationId(current_time, /*unique_id=*/1);
  }

  // Threshold is 100, kTime1 should be dropped, kTime1+10 should not.
  EXPECT_EQ(kTime1.ToInternalValue(),
            mapper.GetLatestGlobalId(kTime1.ToInternalValue()));
  EXPECT_EQ(current_time.ToInternalValue(),
            mapper.GetLatestGlobalId(10 + kTime1.ToInternalValue()));
}

// Tests that subscribers to AddGlobalIdChangeObserver are notified when a
// global_id is noticed to have been changed.
TEST(SessionsGlobalIdMapperTest, AddObserver) {
  SessionsGlobalIdMapper mapper;

  mapper.TrackNavigationId(kTime1, /*unique_id=*/1);

  base::MockCallback<syncer::GlobalIdChange> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  mapper.AddGlobalIdChangeObserver(mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(kTime1.ToInternalValue(), kTime2.ToInternalValue()))
      .Times(0);
}

}  // namespace
}  // namespace sync_sessions

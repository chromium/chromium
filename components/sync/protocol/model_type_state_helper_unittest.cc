// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/model_type_state_helper.h"

#include "components/sync/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

constexpr ModelType kRegularType = ModelType::BOOKMARKS;
constexpr ModelType kCommitOnlyType = ModelType::USER_EVENTS;
constexpr ModelType kApplyUpdatesImmediatelyType = ModelType::HISTORY;

TEST(ModelTypeStateHelperTest, DoesNotMigrateDefaultInstance) {
  sync_pb::ModelTypeState state;

  EXPECT_FALSE(MigrateLegacyInitialSyncDone(state, kRegularType));
  EXPECT_EQ(state.ByteSizeLong(), 0u);

  EXPECT_FALSE(MigrateLegacyInitialSyncDone(state, kCommitOnlyType));
  EXPECT_EQ(state.ByteSizeLong(), 0u);

  EXPECT_FALSE(
      MigrateLegacyInitialSyncDone(state, kApplyUpdatesImmediatelyType));
  EXPECT_EQ(state.ByteSizeLong(), 0u);
}

TEST(ModelTypeStateHelperTest, MigratesRegularModelType) {
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done_deprecated(true);

  EXPECT_TRUE(MigrateLegacyInitialSyncDone(state, kRegularType));
  EXPECT_EQ(state.initial_sync_state(),
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  // The legacy field should also still be there.
  EXPECT_TRUE(state.initial_sync_done_deprecated());

  // The migration should be idempotent.
  EXPECT_FALSE(MigrateLegacyInitialSyncDone(state, kRegularType));
}

TEST(ModelTypeStateHelperTest, MigratesCommitOnlyModelType) {
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done_deprecated(true);

  EXPECT_TRUE(MigrateLegacyInitialSyncDone(state, kCommitOnlyType));
  EXPECT_EQ(state.initial_sync_state(),
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY);
  // The legacy field should also still be there.
  EXPECT_TRUE(state.initial_sync_done_deprecated());

  // The migration should be idempotent.
  EXPECT_FALSE(MigrateLegacyInitialSyncDone(state, kCommitOnlyType));
}

TEST(ModelTypeStateHelperTest, MigratesApplyImmediatelyModelType) {
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done_deprecated(true);

  EXPECT_TRUE(
      MigrateLegacyInitialSyncDone(state, kApplyUpdatesImmediatelyType));
  // Note: For ApplyUpdatesImmediatelyTypes(), `initial_sync_done_deprecated`
  // could map to either `INITIAL_SYNC_DONE` or `INITIAL_SYNC_PARTIALLY_DONE`.
  // To preserve the previous behavior, it gets migrated to `INITIAL_SYNC_DONE`.
  EXPECT_EQ(state.initial_sync_state(),
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  // The legacy field should also still be there.
  EXPECT_TRUE(state.initial_sync_done_deprecated());

  // The migration should be idempotent.
  EXPECT_FALSE(
      MigrateLegacyInitialSyncDone(state, kApplyUpdatesImmediatelyType));
}

}  // namespace
}  // namespace syncer

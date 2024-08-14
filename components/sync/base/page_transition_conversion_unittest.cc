// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/page_transition_conversion.h"

#include "components/sync/protocol/sync_enums.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace syncer {
namespace {

TEST(PageTransitionConversionTest, Roundtrip) {
  for (uint32_t transition_int = ui::PAGE_TRANSITION_FIRST;
       transition_int <= ui::PAGE_TRANSITION_LAST_CORE; transition_int++) {
    ui::PageTransition transition = ui::PageTransitionFromInt(transition_int);

    sync_pb::SyncEnums_PageTransition sync_transition =
        ToSyncPageTransition(transition);
    ui::PageTransition recovered_transition =
        FromSyncPageTransition(sync_transition);
    // Converting to sync's format and back should be lossless.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        transition, recovered_transition));
  }
}

TEST(PageTransitionConversionTest, StripsQualifiers) {
  // Create a ui::PageTransition with a bunch of qualifiers.
  ui::PageTransition transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END | ui::PAGE_TRANSITION_CLIENT_REDIRECT |
      ui::PAGE_TRANSITION_FORWARD_BACK);

  // Converting to sync's format should work, but strip all the qualifiers.
  sync_pb::SyncEnums_PageTransition sync_transition =
      ToSyncPageTransition(transition);
  EXPECT_EQ(sync_transition, sync_pb::SyncEnums_PageTransition_TYPED);
}

}  // namespace
}  // namespace syncer

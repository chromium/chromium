// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/visit_id_remapper.h"

#include "components/history/core/browser/sync/test_history_backend_for_sync.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

constexpr char kOriginatorCacheGuid[] = "originator";

void AddVisitToRemapper(VisitIDRemapper* remapper, const VisitRow& row) {
  remapper->RegisterVisit(
      row.visit_id, row.originator_cache_guid, row.originator_visit_id,
      row.originator_referring_visit, row.originator_opener_visit);
}

class VisitIDRemapperTest : public testing::Test {
 public:
  VisitIDRemapperTest() = default;
  ~VisitIDRemapperTest() override = default;

  VisitRow AddVisitToBackend(const std::string& originator_cache_guid,
                             VisitID originator_visit_id,
                             VisitID originator_referring_visit_id,
                             VisitID originator_opener_visit_id,
                             int transition = ui::PAGE_TRANSITION_LINK |
                                              ui::PAGE_TRANSITION_CHAIN_START |
                                              ui::PAGE_TRANSITION_CHAIN_END) {
    VisitRow row;
    row.originator_cache_guid = originator_cache_guid;
    row.originator_visit_id = originator_visit_id;
    row.originator_referring_visit = originator_referring_visit_id;
    row.originator_opener_visit = originator_opener_visit_id;
    row.transition = ui::PageTransitionFromInt(transition);
    row.visit_id = backend_.AddVisit(row);
    return row;
  }

  TestHistoryBackendForSync* backend() { return &backend_; }

 private:
  TestHistoryBackendForSync backend_;
};

TEST_F(VisitIDRemapperTest, RemapsFromMemory) {
  // Three visits: The second is referred by the first, and the third is opened
  // by the second.
  VisitRow added_visit0 = AddVisitToBackend(kOriginatorCacheGuid, 101, 0, 0);
  VisitRow added_visit1 = AddVisitToBackend(
      kOriginatorCacheGuid, 102, added_visit0.originator_visit_id, 0);
  VisitRow added_visit2 = AddVisitToBackend(kOriginatorCacheGuid, 103, 0,
                                            added_visit1.originator_visit_id);

  // Run the remapper.
  VisitIDRemapper remapper(backend());
  // The visits are not guaranteed to be added in order, so add them in an
  // arbitrary order here.
  AddVisitToRemapper(&remapper, added_visit2);
  AddVisitToRemapper(&remapper, added_visit0);
  AddVisitToRemapper(&remapper, added_visit1);
  remapper.RemapIDs();

  const std::vector<VisitRow>& visits = backend()->GetVisits();
  ASSERT_EQ(visits.size(), 3u);

  // Make sure that the (non-originator) referring/opener_visits were set.
  ASSERT_EQ(visits[0].visit_id, added_visit0.visit_id);
  EXPECT_EQ(visits[0].referring_visit, 0);
  EXPECT_EQ(visits[0].opener_visit, 0);

  ASSERT_EQ(visits[1].visit_id, added_visit1.visit_id);
  EXPECT_EQ(visits[1].referring_visit, added_visit0.visit_id);
  EXPECT_EQ(visits[1].opener_visit, 0);

  ASSERT_EQ(visits[2].visit_id, added_visit2.visit_id);
  EXPECT_EQ(visits[2].referring_visit, 0);
  EXPECT_EQ(visits[2].opener_visit, added_visit1.visit_id);

  // There should have been no queries to the DB - all the remapping should've
  // been done in memory.
  EXPECT_EQ(backend()->get_foreign_visit_call_count(), 0);
}

TEST_F(VisitIDRemapperTest, RemapsFromDB) {
  // Two visits already exist in the DB.
  VisitRow existing_visit0 = AddVisitToBackend(kOriginatorCacheGuid, 101, 0, 0);
  VisitRow existing_visit1 = AddVisitToBackend(kOriginatorCacheGuid, 102, 0, 0);
  // Two visits are newly added, pointing to the two pre-existing visits as
  // their referrer/opener, respectively.
  VisitRow added_visit0 = AddVisitToBackend(
      kOriginatorCacheGuid, 201, existing_visit0.originator_visit_id, 0);
  VisitRow added_visit1 = AddVisitToBackend(
      kOriginatorCacheGuid, 202, 0, existing_visit1.originator_visit_id);

  // Run the remapper.
  VisitIDRemapper remapper(backend());
  // The visits are not guaranteed to be added in order, so add them in an
  // arbitrary order here.
  AddVisitToRemapper(&remapper, added_visit1);
  AddVisitToRemapper(&remapper, added_visit0);
  remapper.RemapIDs();

  const std::vector<VisitRow>& visits = backend()->GetVisits();
  ASSERT_EQ(visits.size(), 4u);

  // Make sure that the (non-originator) referring/opener_visits were set.
  ASSERT_EQ(visits[0].visit_id, existing_visit0.visit_id);
  ASSERT_EQ(visits[1].visit_id, existing_visit1.visit_id);

  ASSERT_EQ(visits[2].visit_id, added_visit0.visit_id);
  EXPECT_EQ(visits[2].referring_visit, existing_visit0.visit_id);
  EXPECT_EQ(visits[2].opener_visit, 0);

  ASSERT_EQ(visits[3].visit_id, added_visit1.visit_id);
  EXPECT_EQ(visits[3].referring_visit, 0);
  EXPECT_EQ(visits[3].opener_visit, existing_visit1.visit_id);

  // There should have been some queries to the DB (as opposed to the
  // RemapsFromMemory test).
  EXPECT_NE(backend()->get_foreign_visit_call_count(), 0);
}

TEST_F(VisitIDRemapperTest, RemapsReferrerChain) {
  // Situation: There's a redirect chain of three visits. The first visit of the
  // chain was opened by a previous visit. Additionally, a later visit was
  // opened by the last visit of the chain.
  VisitRow visit_first = AddVisitToBackend(kOriginatorCacheGuid, 101, 0, 0);
  VisitRow visit_chain0 = AddVisitToBackend(
      kOriginatorCacheGuid, 201, 0, visit_first.originator_visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START));
  VisitRow visit_chain1 = AddVisitToBackend(
      kOriginatorCacheGuid, 202, visit_chain0.originator_visit_id, 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT));
  VisitRow visit_chain2 = AddVisitToBackend(
      kOriginatorCacheGuid, 203, visit_chain1.originator_visit_id, 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END));
  VisitRow visit_last = AddVisitToBackend(kOriginatorCacheGuid, 301, 0,
                                          visit_chain2.originator_visit_id);

  // Run the remapper.
  VisitIDRemapper remapper(backend());
  // The visits are not guaranteed to be added in order, so add them in an
  // arbitrary order here.
  AddVisitToRemapper(&remapper, visit_chain0);
  AddVisitToRemapper(&remapper, visit_chain1);
  AddVisitToRemapper(&remapper, visit_chain2);
  AddVisitToRemapper(&remapper, visit_last);
  AddVisitToRemapper(&remapper, visit_first);
  remapper.RemapIDs();

  const std::vector<VisitRow>& visits = backend()->GetVisits();
  ASSERT_EQ(visits.size(), 5u);

  // Make sure that the (non-originator) referring/opener_visits were set.
  ASSERT_EQ(visits[0].visit_id, visit_first.visit_id);

  ASSERT_EQ(visits[1].visit_id, visit_chain0.visit_id);
  EXPECT_EQ(visits[1].referring_visit, 0);
  EXPECT_EQ(visits[1].opener_visit, visit_first.visit_id);

  ASSERT_EQ(visits[2].visit_id, visit_chain1.visit_id);
  EXPECT_EQ(visits[2].referring_visit, visit_chain0.visit_id);
  EXPECT_EQ(visits[2].opener_visit, 0);

  ASSERT_EQ(visits[3].visit_id, visit_chain2.visit_id);
  EXPECT_EQ(visits[3].referring_visit, visit_chain1.visit_id);
  EXPECT_EQ(visits[3].opener_visit, 0);

  ASSERT_EQ(visits[4].visit_id, visit_last.visit_id);
  EXPECT_EQ(visits[4].referring_visit, 0);
  EXPECT_EQ(visits[4].opener_visit, visit_chain2.visit_id);
}

TEST_F(VisitIDRemapperTest, DoesNotRemapAcrossOriginators) {
  // One pre-existing visit from "originator".
  VisitRow existing_visit0 = AddVisitToBackend(kOriginatorCacheGuid, 101, 0, 0);
  // Three newly-added visits: One from "originator".
  VisitRow added_visit0 = AddVisitToBackend(kOriginatorCacheGuid, 102, 0, 0);
  // ...and two more from a different originator, with referrer/opener IDs which
  // happen to match the previous two visits.
  VisitRow added_visit1 = AddVisitToBackend(
      "different", 103, existing_visit0.originator_visit_id, 0);
  VisitRow added_visit2 =
      AddVisitToBackend("different", 104, 0, added_visit0.originator_visit_id);

  // Run the remapper.
  VisitIDRemapper remapper(backend());
  AddVisitToRemapper(&remapper, added_visit0);
  AddVisitToRemapper(&remapper, added_visit1);
  AddVisitToRemapper(&remapper, added_visit2);
  remapper.RemapIDs();

  // The referrer IDs should *not* have been remapped, since the originator was
  // different.
  const std::vector<VisitRow>& visits = backend()->GetVisits();
  ASSERT_EQ(visits.size(), 4u);
  EXPECT_EQ(visits[0].referring_visit, 0);
  EXPECT_EQ(visits[0].opener_visit, 0);
  EXPECT_EQ(visits[1].referring_visit, 0);
  EXPECT_EQ(visits[1].opener_visit, 0);
  EXPECT_EQ(visits[2].referring_visit, 0);
  EXPECT_EQ(visits[2].opener_visit, 0);
  EXPECT_EQ(visits[3].referring_visit, 0);
  EXPECT_EQ(visits[3].opener_visit, 0);
}

}  // namespace

}  // namespace history

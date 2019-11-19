// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/lost_navigations_recorder.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_directory_setter_upper.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::syncable::Entry;
using syncer::syncable::Id;
using syncer::syncable::MutableEntry;
using syncer::syncable::WriteTransaction;

namespace sync_sessions {
namespace {

const char kTab1SyncTag[] = "tab-YWRkcjHvv74=";
const char kTab2SyncTag[] = "tab-2FyZDHvv74=";

class LostNavigationsRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    dir_maker_.SetUp();
    _id = 1;
  }

  void TearDown() override { dir_maker_.TearDown(); }

  syncer::syncable::Directory* directory() { return dir_maker_.directory(); }

  LostNavigationsRecorder* recorder() { return &recorder_; }

  void AddNavigation(sync_pb::SessionSpecifics* tab_base,
                     int id_override = -1) {
    sync_pb::SessionTab* tab = tab_base->mutable_tab();
    sync_pb::TabNavigation* navigation = tab->add_navigation();
    navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
    navigation->set_unique_id(id_override > 0 ? id_override : _id++);
  }

  void BuildTabSpecifics(const std::string& tag,
                         int tab_id,
                         sync_pb::SessionSpecifics* tab_base,
                         int num_unique_navs = 1) {
    tab_base->set_session_tag(tag);
    tab_base->set_tab_node_id(0);
    sync_pb::SessionTab* tab = tab_base->mutable_tab();
    tab->set_tab_id(tab_id);
    tab->set_current_navigation_index(0);
    for (int i = 0; i < num_unique_navs; ++i) {
      AddNavigation(tab_base);
    }
  }

  void BuildWindowSpecifics(int window_id,
                            sync_pb::SessionSpecifics* window_base) {
    sync_pb::SessionHeader* header = window_base->mutable_header();
    sync_pb::SessionWindow* window = header->add_window();
    window->set_window_id(window_id);
  }

  const Id& CreateEntry() {
    WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
    MutableEntry mutable_entry(&wtrans, syncer::syncable::CREATE,
                               syncer::SESSIONS, wtrans.root_id(),
                               "entrynamutable_entry");
    EXPECT_TRUE(mutable_entry.good());
    return mutable_entry.GetId();
  }

  const syncer::SyncData CreateLocalData(
      const sync_pb::SessionSpecifics& specifics,
      const std::string& sync_tag) const {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(specifics);
    return syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity);
  }

  syncer::SyncChange MakeChange(const std::string& sync_tag,
                                const sync_pb::SessionSpecifics& specifics,
                                syncer::SyncChange::SyncChangeType type) const {
    return syncer::SyncChange(FROM_HERE, type,
                              CreateLocalData(specifics, sync_tag));
  }

  void RecordChange(const Entry* entry, int num_unique_navs) {
    sync_pb::EntitySpecifics specifics;
    BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(),
                      num_unique_navs);
    RecordChange(entry, specifics);
  }

  void RecordChange(const Entry* entry, sync_pb::EntitySpecifics specifics) {
    syncer::SyncChange change = MakeChange(kTab1SyncTag, specifics.session(),
                                           syncer::SyncChange::ACTION_UPDATE);
    recorder()->OnLocalChange(entry, change);
  }

  void TriggerReconcile(MutableEntry* mutable_entry,
                        bool trigger_by_syncing = true,
                        sync_pb::EntitySpecifics* specifics = nullptr) {
    mutable_entry->PutSyncing(trigger_by_syncing);
    mutable_entry->PutIsUnsynced(trigger_by_syncing);
    if (specifics == nullptr) {
      RecordChange(mutable_entry, 0);
    } else {
      RecordChange(mutable_entry, *specifics);
    }
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  int _id;
  LostNavigationsRecorder recorder_;
  syncer::TestDirectorySetterUpper dir_maker_;
};

TEST_F(LostNavigationsRecorderTest, MultipleNavsNoneLost) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  mutable_entry.PutIsUnsynced(true);

  RecordChange(&mutable_entry, 6);

  TriggerReconcile(&mutable_entry);
  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 0, 1);
}

TEST_F(LostNavigationsRecorderTest, MultipleNavsOneLost) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  mutable_entry.PutIsUnsynced(true);

  RecordChange(&mutable_entry, 1);
  RecordChange(&mutable_entry, 1);

  TriggerReconcile(&mutable_entry);

  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 1, 1);
}

TEST_F(LostNavigationsRecorderTest, MultipleNavsMultipleLost) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  mutable_entry.PutIsUnsynced(true);

  RecordChange(&mutable_entry, 5);
  RecordChange(&mutable_entry, 1);

  TriggerReconcile(&mutable_entry);
  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 5, 1);
}

TEST_F(LostNavigationsRecorderTest, MultipleWritesWhileSyncing) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);

  sync_pb::EntitySpecifics specifics;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 5);
  mutable_entry.PutIsUnsynced(true);
  RecordChange(&mutable_entry, specifics);

  mutable_entry.PutSyncing(true);
  RecordChange(&mutable_entry, specifics);

  specifics.mutable_session()->mutable_tab()->clear_navigation();
  for (int i = 0; i < 5; i++) {
    AddNavigation(specifics.mutable_session());
    RecordChange(&mutable_entry, specifics);
  }

  TriggerReconcile(&mutable_entry, false);
  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 0, 1);
}

TEST_F(LostNavigationsRecorderTest, MultipleWritesMultipleEntriesNoneLost) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  const Id& id2 = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  MutableEntry mutable_entry2(&wtrans, syncer::syncable::GET_BY_ID, id2);
  mutable_entry.PutIsUnsynced(true);
  mutable_entry2.PutIsUnsynced(true);

  sync_pb::EntitySpecifics specifics;
  sync_pb::EntitySpecifics specifics2;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 5);
  BuildTabSpecifics(kTab2SyncTag, 2, specifics2.mutable_session(), 5);
  RecordChange(&mutable_entry, specifics);
  RecordChange(&mutable_entry2, specifics2);

  mutable_entry.PutSyncing(true);
  mutable_entry2.PutSyncing(true);
  RecordChange(&mutable_entry, specifics);
  RecordChange(&mutable_entry2, specifics2);

  specifics.mutable_session()->mutable_tab()->clear_navigation();
  specifics2.mutable_session()->mutable_tab()->clear_navigation();
  for (int i = 0; i < 5; i++) {
    AddNavigation(specifics.mutable_session());
    AddNavigation(specifics2.mutable_session());

    RecordChange(&mutable_entry, specifics);
    RecordChange(&mutable_entry2, specifics2);
  }

  TriggerReconcile(&mutable_entry, false, &specifics);
  TriggerReconcile(&mutable_entry2, false, &specifics2);

  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 0, 4);
}

TEST_F(LostNavigationsRecorderTest, MultipleWritesMultipleEntriesMultipleLost) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  const Id& id2 = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  MutableEntry mutable_entry2(&wtrans, syncer::syncable::GET_BY_ID, id2);
  mutable_entry.PutIsUnsynced(true);
  mutable_entry2.PutIsUnsynced(true);

  sync_pb::EntitySpecifics specifics;
  sync_pb::EntitySpecifics specifics2;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 5);
  BuildTabSpecifics(kTab2SyncTag, 2, specifics2.mutable_session(), 5);
  RecordChange(&mutable_entry, specifics);
  RecordChange(&mutable_entry2, specifics2);

  mutable_entry.PutSyncing(true);
  mutable_entry2.PutSyncing(true);
  RecordChange(&mutable_entry, specifics);
  RecordChange(&mutable_entry2, specifics2);

  specifics.mutable_session()->mutable_tab()->clear_navigation();
  specifics2.mutable_session()->mutable_tab()->clear_navigation();
  for (int i = 0; i < 5; i++) {
    AddNavigation(specifics.mutable_session());
    AddNavigation(specifics2.mutable_session());

    RecordChange(&mutable_entry, specifics);
    RecordChange(&mutable_entry2, specifics2);
  }

  specifics.mutable_session()
      ->mutable_tab()
      ->mutable_navigation()
      ->DeleteSubrange(0, 2);
  specifics2.mutable_session()
      ->mutable_tab()
      ->mutable_navigation()
      ->DeleteSubrange(0, 2);
  RecordChange(&mutable_entry, specifics);
  RecordChange(&mutable_entry, specifics2);

  TriggerReconcile(&mutable_entry, false, &specifics);
  TriggerReconcile(&mutable_entry2, false, &specifics2);

  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 2, 2);
}

TEST_F(LostNavigationsRecorderTest, NoWritesWhileSyncingMultipleLost) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);

  sync_pb::EntitySpecifics specifics;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 5);
  syncer::SyncChange change = MakeChange(kTab1SyncTag, specifics.session(),
                                         syncer::SyncChange::ACTION_UPDATE);
  mutable_entry.PutIsUnsynced(true);
  recorder()->OnLocalChange(&mutable_entry, change);

  specifics.mutable_session()->mutable_tab()->clear_navigation();
  AddNavigation(specifics.mutable_session());
  change = MakeChange(kTab1SyncTag, specifics.session(),
                      syncer::SyncChange::ACTION_UPDATE);
  recorder()->OnLocalChange(&mutable_entry, change);

  mutable_entry.PutIsUnsynced(false);
  recorder()->OnLocalChange(&mutable_entry, change);
  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 5, 1);
}

TEST_F(LostNavigationsRecorderTest, WindowChangeDoesNotTriggerReconcile) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  const Id& id2 = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  MutableEntry mutable_entry2(&wtrans, syncer::syncable::GET_BY_ID, id2);
  mutable_entry.PutIsUnsynced(true);

  RecordChange(&mutable_entry, 1);

  sync_pb::EntitySpecifics specifics;
  BuildWindowSpecifics(1, specifics.mutable_session());
  RecordChange(&mutable_entry2, specifics);

  EXPECT_EQ(0ul,
            histogram_tester.GetAllSamples("Sync.LostNavigationCount").size());
}

TEST_F(LostNavigationsRecorderTest, Samutable_entryNavigationSetAcrossStates) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);

  sync_pb::EntitySpecifics specifics;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 5);
  syncer::SyncChange change = MakeChange(kTab1SyncTag, specifics.session(),
                                         syncer::SyncChange::ACTION_UPDATE);
  mutable_entry.PutIsUnsynced(true);
  recorder()->OnLocalChange(&mutable_entry, change);
  recorder()->OnLocalChange(&mutable_entry, change);

  mutable_entry.PutIsUnsynced(false);
  recorder()->OnLocalChange(&mutable_entry, change);
  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 0, 1);
}

TEST_F(LostNavigationsRecorderTest, RevisitPreviousNavs) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  mutable_entry.PutIsUnsynced(true);

  sync_pb::EntitySpecifics specifics;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 3);
  RecordChange(&mutable_entry, specifics);

  AddNavigation(specifics.mutable_session());
  RecordChange(&mutable_entry, specifics);

  specifics.mutable_session()
      ->mutable_tab()
      ->mutable_navigation()
      ->RemoveLast();
  RecordChange(&mutable_entry, specifics);

  TriggerReconcile(&mutable_entry, true, &specifics);

  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 1, 1);
}

TEST_F(LostNavigationsRecorderTest, MultipleNavsMultipleLostWithOverlap) {
  base::HistogramTester histogram_tester;
  const Id& id = CreateEntry();
  WriteTransaction wtrans(FROM_HERE, syncer::syncable::UNITTEST, directory());
  MutableEntry mutable_entry(&wtrans, syncer::syncable::GET_BY_ID, id);
  mutable_entry.PutIsUnsynced(true);

  sync_pb::EntitySpecifics specifics;
  BuildTabSpecifics(kTab1SyncTag, 1, specifics.mutable_session(), 5);
  RecordChange(&mutable_entry, specifics);

  AddNavigation(specifics.mutable_session());
  AddNavigation(specifics.mutable_session());

  specifics.mutable_session()
      ->mutable_tab()
      ->mutable_navigation()
      ->DeleteSubrange(0, 2);
  RecordChange(&mutable_entry, specifics);

  TriggerReconcile(&mutable_entry, true, &specifics);

  histogram_tester.ExpectBucketCount("Sync.LostNavigationCount", 2, 1);
}

}  // namespace
}  // namespace sync_sessions

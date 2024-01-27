// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/persistent_event_store.h"

#include <map>
#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

void VerifyEventsInListAndMap(const std::map<std::string, Event>& map,
                              const std::vector<Event>& list) {
  ASSERT_EQ(map.size(), list.size());

  for (const auto& event : list) {
    const auto& it = map.find(event.name());
    ASSERT_NE(map.end(), it);
    test::VerifyEventsEqual(&event, &it->second);
  }
}

class PersistentEventStoreTest : public ::testing::Test {
 public:
  PersistentEventStoreTest() : db_(nullptr) {
    load_callback_ = base::BindOnce(&PersistentEventStoreTest::LoadCallback,
                                    base::Unretained(this));
  }

  void TearDown() override {
    db_events_.clear();
    db_ = nullptr;
    store_.reset();
  }

 protected:
  void SetUpDB() {
    DCHECK(!db_);
    DCHECK(!store_);

    auto db = std::make_unique<leveldb_proto::test::FakeDB<Event>>(&db_events_);
    db_ = db.get();
    store_ = std::make_unique<PersistentEventStore>(std::move(db));
  }

  void LoadCallback(bool success, std::unique_ptr<std::vector<Event>> events) {
    load_successful_ = success;
    load_results_ = std::move(events);
  }

  // Callback results.
  std::optional<bool> load_successful_;
  std::unique_ptr<std::vector<Event>> load_results_;

  EventStore::OnLoadedCallback load_callback_;
  std::map<std::string, Event> db_events_;
  raw_ptr<leveldb_proto::test::FakeDB<Event>> db_;
  std::unique_ptr<EventStore> store_;
};

}  // namespace

TEST_F(PersistentEventStoreTest, SuccessfulInitAndLoadEmptyStore) {
  SetUpDB();

  base::HistogramTester histogram_tester;

  store_->Load(std::move(load_callback_));
  // The initialize should not trigger a response to the callback.
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  // The load should trigger a response to the callback.
  db_->LoadCallback(true);
  EXPECT_TRUE(load_successful_.value());

  // Validate that we have no entries.
  EXPECT_NE(nullptr, load_results_);
  EXPECT_TRUE(load_results_->empty());

  // Verify histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Load." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.TotalEvents", 0, 1);
}

TEST_F(PersistentEventStoreTest, SuccessfulInitAndLoadWithEvents) {
  // Populate fake Event entries.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 1, 1);

  Event event2;
  event2.set_name("event2");
  test::SetEventCountForDay(&event2, 1, 3);
  test::SetEventCountForDay(&event2, 2, 5);

  db_events_.insert(std::pair<std::string, Event>(event1.name(), event1));
  db_events_.insert(std::pair<std::string, Event>(event2.name(), event2));

  SetUpDB();

  base::HistogramTester histogram_tester;

  // The initialize should not trigger a response to the callback.
  store_->Load(std::move(load_callback_));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  // The load should trigger a response to the callback.
  db_->LoadCallback(true);
  EXPECT_TRUE(load_successful_.value());
  EXPECT_NE(nullptr, load_results_);

  // Validate that we have the two events that we expect.
  VerifyEventsInListAndMap(db_events_, *load_results_);

  // Verify histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Load." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.TotalEvents", 3, 1);
}

TEST_F(PersistentEventStoreTest, SuccessfulInitBadLoad) {
  base::HistogramTester histogram_tester;
  SetUpDB();

  store_->Load(std::move(load_callback_));

  // The initialize should not trigger a response to the callback.
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(load_successful_.has_value());

  // The load will fail and should trigger the callback.
  db_->LoadCallback(false);
  EXPECT_FALSE(load_successful_.value());
  EXPECT_FALSE(store_->IsReady());

  // Histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Load." + suffix, 0, 1);
  histogram_tester.ExpectTotalCount("InProductHelp.Db.TotalEvents", 0);
}

TEST_F(PersistentEventStoreTest, BadInit) {
  base::HistogramTester histogram_tester;
  SetUpDB();

  store_->Load(std::move(load_callback_));

  // The initialize will fail and should trigger the callback.
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
  EXPECT_FALSE(load_successful_.value());
  EXPECT_FALSE(store_->IsReady());

  // Histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 0, 1);
  histogram_tester.ExpectTotalCount("InProductHelp.Db.Load." + suffix, 0);
  histogram_tester.ExpectTotalCount("InProductHelp.Db.TotalEvents", 0);
}

TEST_F(PersistentEventStoreTest, IsReady) {
  SetUpDB();
  EXPECT_FALSE(store_->IsReady());

  store_->Load(std::move(load_callback_));
  EXPECT_FALSE(store_->IsReady());

  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_FALSE(store_->IsReady());

  db_->LoadCallback(true);
  EXPECT_TRUE(store_->IsReady());
}

TEST_F(PersistentEventStoreTest, WriteEvent) {
  SetUpDB();

  store_->Load(std::move(load_callback_));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db_->LoadCallback(true);

  Event event;
  event.set_name("event");
  test::SetEventCountForDay(&event, 1, 2);

  store_->WriteEvent(event);
  db_->UpdateCallback(true);

  EXPECT_EQ(1U, db_events_.size());

  const auto& it = db_events_.find("event");
  EXPECT_NE(db_events_.end(), it);
  test::VerifyEventsEqual(&event, &it->second);
}

TEST_F(PersistentEventStoreTest, WriteAndDeleteEvent) {
  SetUpDB();

  store_->Load(std::move(load_callback_));
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db_->LoadCallback(true);

  Event event;
  event.set_name("event");
  test::SetEventCountForDay(&event, 1, 2);

  store_->WriteEvent(event);
  db_->UpdateCallback(true);

  EXPECT_EQ(1U, db_events_.size());

  store_->DeleteEvent("event");
  db_->UpdateCallback(true);

  const auto& it = db_events_.find("event");
  EXPECT_EQ(db_events_.end(), it);
}

}  // namespace feature_engagement

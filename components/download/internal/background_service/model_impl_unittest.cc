// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/model_impl.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/internal/background_service/test/entry_utils.h"
#include "components/download/internal/background_service/test/mock_model_client.h"
#include "components/download/internal/background_service/test/test_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace download {

namespace {

class DownloadServiceModelImplTest : public testing::Test {
 public:
  DownloadServiceModelImplTest() : store_(nullptr) {}

  ~DownloadServiceModelImplTest() override = default;

  void SetUp() override {
    auto store = std::make_unique<test::TestStore>();
    store_ = store.get();
    model_ = std::make_unique<ModelImpl>(std::move(store));
  }

 protected:
  test::MockModelClient client_;
  test::TestStore* store_;
  std::unique_ptr<ModelImpl> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadServiceModelImplTest);
};

}  // namespace

TEST_F(DownloadServiceModelImplTest, SuccessfulLifecycle) {
  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
}

TEST_F(DownloadServiceModelImplTest, SuccessfulInitWithEntries) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry1, entry2};

  base::HistogramTester histogram_tester;
  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));

  EXPECT_TRUE(test::CompareEntry(&entry1, model_->Get(entry1.guid)));
  EXPECT_TRUE(test::CompareEntry(&entry2, model_->Get(entry2.guid)));

  // Verify histograms.
  histogram_tester.ExpectBucketCount("Download.Service.Db.Records", 2, 1);
  histogram_tester.ExpectBucketCount("Download.Service.Db.Records.New", 2, 1);
  histogram_tester.ExpectBucketCount("Download.Service.Db.Records.Available", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Download.Service.Db.Records.Active", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Download.Service.Db.Records.Paused", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Download.Service.Db.Records.Complete", 0,
                                     1);
}

TEST_F(DownloadServiceModelImplTest, BadInit) {
  EXPECT_CALL(client_, OnModelReady(false)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(false, std::make_unique<std::vector<Entry>>());
}

TEST_F(DownloadServiceModelImplTest, HardRecoverGoodModel) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry1, entry2};

  EXPECT_CALL(client_, OnModelReady(true)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));

  EXPECT_CALL(client_, OnModelHardRecoverComplete(true));

  model_->HardRecover();
  store_->TriggerHardRecover(true);
  EXPECT_TRUE(model_->PeekEntries().empty());
}

TEST_F(DownloadServiceModelImplTest, HardRecoverBadModel) {
  EXPECT_CALL(client_, OnModelReady(false)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(false, std::make_unique<std::vector<Entry>>());

  EXPECT_CALL(client_, OnModelHardRecoverComplete(true));

  model_->HardRecover();
  store_->TriggerHardRecover(true);
  EXPECT_TRUE(model_->PeekEntries().empty());
}

TEST_F(DownloadServiceModelImplTest, HardRecoverFailsGoodModel) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry1, entry2};

  EXPECT_CALL(client_, OnModelReady(true)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));

  EXPECT_CALL(client_, OnModelHardRecoverComplete(false));

  model_->HardRecover();
  store_->TriggerHardRecover(false);
  EXPECT_TRUE(model_->PeekEntries().empty());
}

TEST_F(DownloadServiceModelImplTest, HardRecoverFailsBadModel) {
  EXPECT_CALL(client_, OnModelReady(false)).Times(1);

  model_->Initialize(&client_);
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(false, std::make_unique<std::vector<Entry>>());

  EXPECT_CALL(client_, OnModelHardRecoverComplete(false));

  model_->HardRecover();
  store_->TriggerHardRecover(false);
  EXPECT_TRUE(model_->PeekEntries().empty());
}

TEST_F(DownloadServiceModelImplTest, Add) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);
  EXPECT_CALL(client_, OnItemAdded(true, entry1.client, entry1.guid)).Times(1);
  EXPECT_CALL(client_, OnItemAdded(false, entry2.client, entry2.guid)).Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());

  model_->Add(entry1);
  EXPECT_TRUE(test::CompareEntry(&entry1, model_->Get(entry1.guid)));
  EXPECT_TRUE(test::CompareEntry(&entry1, store_->LastUpdatedEntry()));
  store_->TriggerUpdate(true);

  model_->Add(entry2);
  EXPECT_TRUE(test::CompareEntry(&entry2, model_->Get(entry2.guid)));
  EXPECT_TRUE(test::CompareEntry(&entry2, store_->LastUpdatedEntry()));

  store_->TriggerUpdate(false);
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));
}

TEST_F(DownloadServiceModelImplTest, Update) {
  Entry entry1 = test::BuildBasicEntry();

  Entry entry2(entry1);
  entry2.state = Entry::State::AVAILABLE;

  Entry entry3(entry1);
  entry3.state = Entry::State::ACTIVE;

  std::vector<Entry> entries = {entry1};

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);
  EXPECT_CALL(client_, OnItemUpdated(true, entry1.client, entry1.guid))
      .Times(2);
  EXPECT_CALL(client_, OnItemUpdated(false, entry1.client, entry1.guid))
      .Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  std::vector<Entry*> entries_pointers = model_->PeekEntries();

  // Update with a different object.
  model_->Update(entry2);
  EXPECT_TRUE(test::CompareEntry(&entry2, model_->Get(entry2.guid)));
  EXPECT_TRUE(test::CompareEntry(&entry2, store_->LastUpdatedEntry()));
  store_->TriggerUpdate(true);

  // Update with the same object.
  Entry* entry = model_->Get(entry1.guid);
  entry->state = Entry::State::NEW;
  model_->Update(*entry);
  store_->TriggerUpdate(true);
  // Peek entries should return the same set of pointers.
  EXPECT_TRUE(test::CompareEntryList(entries_pointers, model_->PeekEntries()));

  model_->Update(entry3);
  EXPECT_TRUE(test::CompareEntry(&entry3, model_->Get(entry3.guid)));
  EXPECT_TRUE(test::CompareEntry(&entry3, store_->LastUpdatedEntry()));

  store_->TriggerUpdate(false);
  EXPECT_TRUE(test::CompareEntry(&entry3, model_->Get(entry3.guid)));
}

TEST_F(DownloadServiceModelImplTest, Remove) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry1, entry2};

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);
  EXPECT_CALL(client_, OnItemRemoved(true, entry1.client, entry1.guid))
      .Times(1);
  EXPECT_CALL(client_, OnItemRemoved(false, entry2.client, entry2.guid))
      .Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));

  model_->Remove(entry1.guid);
  EXPECT_EQ(entry1.guid, store_->LastRemovedEntry());
  EXPECT_EQ(nullptr, model_->Get(entry1.guid));
  store_->TriggerRemove(true);

  model_->Remove(entry2.guid);
  EXPECT_EQ(entry2.guid, store_->LastRemovedEntry());
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));
  store_->TriggerRemove(false);
}

TEST_F(DownloadServiceModelImplTest, Get) {
  Entry entry = test::BuildBasicEntry();

  std::vector<Entry> entries = {entry};

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));

  EXPECT_TRUE(test::CompareEntry(&entry, model_->Get(entry.guid)));
  EXPECT_EQ(nullptr, model_->Get(base::GenerateGUID()));
}

TEST_F(DownloadServiceModelImplTest, PeekEntries) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry1, entry2};

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));

  std::vector<Entry*> expected_peek = {&entry1, &entry2};

  EXPECT_TRUE(test::CompareEntryList(expected_peek, model_->PeekEntries()));
}

TEST_F(DownloadServiceModelImplTest, TestRemoveAfterAdd) {
  Entry entry = test::BuildBasicEntry();

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);
  EXPECT_CALL(client_, OnItemAdded(_, _, _)).Times(0);
  EXPECT_CALL(client_, OnItemRemoved(true, entry.client, entry.guid)).Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());

  model_->Add(entry);
  EXPECT_TRUE(test::CompareEntry(&entry, model_->Get(entry.guid)));

  model_->Remove(entry.guid);
  EXPECT_EQ(nullptr, model_->Get(entry.guid));

  store_->TriggerUpdate(true);
  store_->TriggerRemove(true);
}

TEST_F(DownloadServiceModelImplTest, TestRemoveAfterUpdate) {
  Entry entry1 = test::BuildBasicEntry();

  Entry entry2(entry1);
  entry2.state = Entry::State::AVAILABLE;

  std::vector<Entry> entries = {entry1};

  InSequence sequence;
  EXPECT_CALL(client_, OnModelReady(true)).Times(1);
  EXPECT_CALL(client_, OnItemUpdated(_, _, _)).Times(0);
  EXPECT_CALL(client_, OnItemRemoved(true, entry1.client, entry1.guid))
      .Times(1);

  model_->Initialize(&client_);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  EXPECT_TRUE(test::CompareEntry(&entry1, model_->Get(entry1.guid)));

  model_->Update(entry2);
  EXPECT_TRUE(test::CompareEntry(&entry2, model_->Get(entry2.guid)));

  model_->Remove(entry2.guid);
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));

  store_->TriggerUpdate(true);
  store_->TriggerRemove(true);
}

}  // namespace download

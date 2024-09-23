// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_store.h"

#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using testing::ElementsAre;
using testing::Pair;

using LoadStreamResult = FeedStore::LoadStreamResult;

std::string KeyForContentId(std::string_view prefix,
                            const feedwire::ContentId& content_id) {
  return base::StrCat({prefix, content_id.content_domain(), ",",
                       base::NumberToString(content_id.type()), ",",
                       base::NumberToString(content_id.id())});
}

feedstore::Record RecordForContent(feedstore::Content content) {
  feedstore::Record record;
  *record.mutable_content() = std::move(content);
  return record;
}

feedstore::Record RecordForSharedState(feedstore::StreamSharedState shared) {
  feedstore::Record record;
  *record.mutable_shared_state() = std::move(shared);
  return record;
}

feedstore::Record RecordForAction(feedstore::StoredAction action) {
  feedstore::Record record;
  *record.mutable_local_action() = std::move(action);
  return record;
}

feedstore::StoredAction MakeAction(int32_t id) {
  feedstore::StoredAction action;
  action.set_id(id);
  return action;
}

feedstore::DocView CreateDocView(uint64_t docid, int64_t view_time_millis) {
  feedstore::DocView view;
  view.set_docid(docid);
  view.set_view_time_millis(view_time_millis);
  return view;
}

}  // namespace

class FeedStoreTest : public testing::Test {
 protected:
  void MakeFeedStore(std::map<std::string, feedstore::Record> entries,
                     leveldb_proto::Enums::InitStatus init_status =
                         leveldb_proto::Enums::InitStatus::kOK) {
    db_entries_ = std::move(entries);
    auto fake_db =
        std::make_unique<leveldb_proto::test::FakeDB<feedstore::Record>>(
            &db_entries_);
    fake_db_ = fake_db.get();
    store_ = std::make_unique<FeedStore>(std::move(fake_db));
    store_->Initialize(base::DoNothing());
    fake_db_->InitStatusCallback(init_status);
  }

  std::set<std::string> StoredKeys() {
    std::set<std::string> result;
    for (auto& entry : db_entries_) {
      result.insert(entry.first);
    }
    return result;
  }

  std::string StoreToString() {
    std::stringstream ss;
    for (auto& entry : db_entries_) {
      ss << "[" << entry.first << "] " << entry.second;
    }
    return ss.str();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FeedStore> store_;
  std::map<std::string, feedstore::Record> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<feedstore::Record>> fake_db_;
};

TEST_F(FeedStoreTest, InitSuccess) {
  MakeFeedStore({});
  EXPECT_TRUE(store_->IsInitializedForTesting());
}

TEST_F(FeedStoreTest, InitFailure) {
  std::map<std::string, feedstore::Record> entries;
  auto fake_db =
      std::make_unique<leveldb_proto::test::FakeDB<feedstore::Record>>(
          &entries);
  leveldb_proto::test::FakeDB<feedstore::Record>* fake_db_raw = fake_db.get();
  auto store = std::make_unique<FeedStore>(std::move(fake_db));

  store->Initialize(base::DoNothing());
  EXPECT_FALSE(store->IsInitializedForTesting());

  fake_db_raw->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
  EXPECT_FALSE(store->IsInitializedForTesting());
}

TEST_F(FeedStoreTest, OverwriteStream) {
  MakeFeedStore({});
  CallbackReceiver<bool> receiver;
  // TODO(harringtond): find a long term fix for assumptions about
  // kTestTimeEpoch value.
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());

  constexpr char want[] = R"([S/i] {
  stream_data {
    content_id {
      content_domain: "root"
    }
    root_event_id: "\b{"
    next_page_token: "page-2"
    shared_state_ids {
      content_domain: "render_data"
    }
    stream_key: "i"
    content_hashes {
      hashes: 1403410510
    }
    content_hashes {
      hashes: 1084072211
    }
  }
}
[T/i/0] {
  stream_structures {
    stream_key: "i"
    structures {
      operation: 1
    }
    structures {
      operation: 2
      content_id {
        content_domain: "root"
      }
      type: 1
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "stories"
        type: 4
      }
      parent_id {
        content_domain: "content"
        type: 3
      }
      type: 3
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 1
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "stories"
        type: 4
        id: 1
      }
      parent_id {
        content_domain: "content"
        type: 3
        id: 1
      }
      type: 3
    }
  }
}
[c/i/stories,4,0] {
  content {
    content_id {
      content_domain: "stories"
      type: 4
    }
    frame: "f:0"
    stream_key: "i"
  }
}
[c/i/stories,4,1] {
  content {
    content_id {
      content_domain: "stories"
      type: 4
      id: 1
    }
    frame: "f:1"
    stream_key: "i"
  }
}
[s/i/render_data,0,0] {
  shared_state {
    content_id {
      content_domain: "render_data"
    }
    shared_state_data: "ss:0"
    stream_key: "i"
  }
}
)";
  EXPECT_STRINGS_EQUAL(want, StoreToString());
}

TEST_F(FeedStoreTest, OverwriteStreamWebFeed) {
  MakeFeedStore({});
  CallbackReceiver<bool> receiver;
  // TODO(harringtond): find a long term fix for assumptions about
  // kTestTimeEpoch value.
  store_->OverwriteStream(StreamType(StreamKind::kFollowing),
                          MakeTypicalInitialModelState(), receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());

  constexpr char want[] = R"([S/w] {
  stream_data {
    content_id {
      content_domain: "root"
    }
    root_event_id: "\b{"
    next_page_token: "page-2"
    shared_state_ids {
      content_domain: "render_data"
    }
    stream_key: "w"
    content_hashes {
      hashes: 1403410510
    }
    content_hashes {
      hashes: 1084072211
    }
  }
}
[T/w/0] {
  stream_structures {
    stream_key: "w"
    structures {
      operation: 1
    }
    structures {
      operation: 2
      content_id {
        content_domain: "root"
      }
      type: 1
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "stories"
        type: 4
      }
      parent_id {
        content_domain: "content"
        type: 3
      }
      type: 3
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 1
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "stories"
        type: 4
        id: 1
      }
      parent_id {
        content_domain: "content"
        type: 3
        id: 1
      }
      type: 3
    }
  }
}
[c/w/stories,4,0] {
  content {
    content_id {
      content_domain: "stories"
      type: 4
    }
    frame: "f:0"
    stream_key: "w"
  }
}
[c/w/stories,4,1] {
  content {
    content_id {
      content_domain: "stories"
      type: 4
      id: 1
    }
    frame: "f:1"
    stream_key: "w"
  }
}
[s/w/render_data,0,0] {
  shared_state {
    content_id {
      content_domain: "render_data"
    }
    shared_state_data: "ss:0"
    stream_key: "w"
  }
}
)";
  EXPECT_STRINGS_EQUAL(want, StoreToString());
}

TEST_F(FeedStoreTest, OverwriteStreamOverwritesData) {
  MakeFeedStore({});
  // Insert some junk that should be removed.
  db_entries_["S/i"].mutable_local_action()->set_id(6);
  db_entries_["T/i/0"].mutable_local_action()->set_id(6);
  db_entries_["T/i/73"].mutable_local_action()->set_id(6);
  db_entries_["c/i/stories,4,0"].mutable_local_action()->set_id(6);
  db_entries_["c/i/stories,4,1"].mutable_local_action()->set_id(6);
  db_entries_["c/i/garbage"].mutable_local_action()->set_id(6);
  db_entries_["s/i/render_data,0,0"].mutable_local_action()->set_id(6);
  db_entries_["s/i/garbage,0,0"].mutable_local_action()->set_id(6);
  // Some junk that should NOT be removed.
  db_entries_["s/1/stories,0,0"].mutable_local_action()->set_id(6);
  db_entries_["S/1"].mutable_local_action()->set_id(6);
  db_entries_["T/1"].mutable_local_action()->set_id(6);
  db_entries_["T/1/0"].mutable_local_action()->set_id(6);
  db_entries_["m"].mutable_local_action()->set_id(6);

  CallbackReceiver<bool> receiver;
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  ASSERT_EQ(
      std::set<std::string>({"S/i", "T/i/0", "c/i/stories,4,0",
                             "c/i/stories,4,1", "s/i/render_data,0,0",
                             "s/1/stories,0,0", "S/1", "T/1", "T/1/0", "m"}),
      StoredKeys());
}

TEST_F(FeedStoreTest, LoadStreamSuccess) {
  MakeFeedStore({});
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(StreamType(StreamKind::kForYou), receiver.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(receiver.GetResult()->read_error);
  EXPECT_EQ(ToTextProto(MakeRootId()),
            ToTextProto(receiver.GetResult()->stream_data.content_id()));
}

TEST_F(FeedStoreTest, LoadStreamFail) {
  MakeFeedStore({});
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(StreamType(StreamKind::kForYou), receiver.Bind());
  fake_db_->LoadCallback(false);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_TRUE(receiver.GetResult()->read_error);
}

TEST_F(FeedStoreTest, LoadStreamNoData) {
  MakeFeedStore({});

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(StreamType(StreamKind::kForYou), receiver.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(receiver.GetResult()->stream_data.has_content_id());
}

TEST_F(FeedStoreTest, LoadStreamIgnoresADifferentStreamType) {
  MakeFeedStore({});
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);

  CallbackReceiver<LoadStreamResult> receiver;
  store_->LoadStream(StreamType(StreamKind::kFollowing), receiver.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(receiver.GetResult()->stream_data.has_content_id());
  EXPECT_TRUE(receiver.GetResult()->stream_structures.empty());
}

TEST_F(FeedStoreTest, WriteOperations) {
  MakeFeedStore({});
  CallbackReceiver<LoadStreamResult> receiver;
  store_->WriteOperations(StreamType(StreamKind::kForYou),
                          /*sequence_number=*/5,
                          {MakeOperation(MakeCluster(2, MakeRootId())),
                           MakeOperation(MakeCluster(6, MakeRootId()))});
  fake_db_->UpdateCallback(true);

  constexpr char want[] = R"([T/i/5] {
  stream_structures {
    stream_key: "i"
    sequence_number: 5
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 2
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
    structures {
      operation: 2
      content_id {
        content_domain: "content"
        type: 3
        id: 6
      }
      parent_id {
        content_domain: "root"
      }
      type: 4
    }
  }
}
)";
  EXPECT_STRINGS_EQUAL(want, StoreToString());
}

TEST_F(FeedStoreTest, ReadNonexistentContentAndSharedStates) {
  MakeFeedStore({});
  CallbackReceiver<std::vector<feedstore::Content>,
                   std::vector<feedstore::StreamSharedState>>
      cr;

  store_->ReadContent(StreamType(StreamKind::kForYou),
                      {MakeContentContentId(0)}, {MakeSharedStateContentId(0)},
                      cr.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_NE(cr.GetResult<0>(), std::nullopt);
  EXPECT_EQ(cr.GetResult<0>()->size(), 0ul);
  ASSERT_NE(cr.GetResult<1>(), std::nullopt);
  EXPECT_EQ(cr.GetResult<1>()->size(), 0ul);
}

TEST_F(FeedStoreTest, ReadContentAndSharedStates) {
  feedstore::Content content1 = MakeContent(1);
  feedstore::Content content2 = MakeContent(2);
  feedstore::StreamSharedState shared1 = MakeSharedState(1);
  feedstore::StreamSharedState shared2 = MakeSharedState(2);

  MakeFeedStore({{KeyForContentId("c/i/", content1.content_id()),
                  RecordForContent(content1)},
                 {KeyForContentId("c/i/", content2.content_id()),
                  RecordForContent(content2)},
                 {KeyForContentId("s/i/", shared1.content_id()),
                  RecordForSharedState(shared1)},
                 {KeyForContentId("s/i/", shared2.content_id()),
                  RecordForSharedState(shared2)}});

  std::vector<feedwire::ContentId> content_ids = {content1.content_id(),
                                                  content2.content_id()};
  std::vector<feedwire::ContentId> shared_state_ids = {shared1.content_id(),
                                                       shared2.content_id()};

  CallbackReceiver<std::vector<feedstore::Content>,
                   std::vector<feedstore::StreamSharedState>>
      cr;

  // Successful read
  store_->ReadContent(StreamType(StreamKind::kForYou), content_ids,
                      shared_state_ids, cr.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_NE(cr.GetResult<0>(), std::nullopt);
  std::vector<feedstore::Content> content = *cr.GetResult<0>();
  ASSERT_NE(cr.GetResult<1>(), std::nullopt);
  std::vector<feedstore::StreamSharedState> shared_states = *cr.GetResult<1>();

  ASSERT_EQ(content.size(), 2ul);
  EXPECT_EQ(ToTextProto(content[0].content_id()),
            ToTextProto(content1.content_id()));
  EXPECT_EQ(content[0].frame(), content1.frame());

  ASSERT_EQ(shared_states.size(), 2ul);
  EXPECT_EQ(ToTextProto(shared_states[0].content_id()),
            ToTextProto(shared1.content_id()));
  EXPECT_EQ(shared_states[0].shared_state_data(), shared1.shared_state_data());

  // Failed read
  cr.Clear();
  store_->ReadContent(StreamType(StreamKind::kForYou), content_ids,
                      shared_state_ids, cr.Bind());
  fake_db_->LoadCallback(false);

  ASSERT_NE(cr.GetResult<0>(), std::nullopt);
  EXPECT_EQ(cr.GetResult<0>()->size(), 0ul);
  ASSERT_NE(cr.GetResult<1>(), std::nullopt);
  EXPECT_EQ(cr.GetResult<1>()->size(), 0ul);
}

TEST_F(FeedStoreTest, ReadActions) {
  MakeFeedStore({{"a/0", RecordForAction(MakeAction(0))},
                 {"a/1", RecordForAction(MakeAction(1))},
                 {"a/2", RecordForAction(MakeAction(2))}});

  // Successful read
  CallbackReceiver<std::vector<feedstore::StoredAction>> receiver;
  store_->ReadActions(receiver.Bind());
  fake_db_->LoadCallback(true);
  ASSERT_NE(std::nullopt, receiver.GetResult());
  std::vector<feedstore::StoredAction> result =
      std::move(*receiver.GetResult());

  EXPECT_EQ(3ul, result.size());
  EXPECT_EQ(2, result[2].id());

  // Failed read
  receiver.Clear();
  store_->ReadActions(receiver.Bind());
  fake_db_->LoadCallback(false);
  ASSERT_NE(std::nullopt, receiver.GetResult());
  result = std::move(*receiver.GetResult());
  EXPECT_EQ(0ul, result.size());
}

TEST_F(FeedStoreTest, WriteActions) {
  MakeFeedStore({});
  feedstore::StoredAction action;

  CallbackReceiver<bool> receiver;
  store_->WriteActions({action}, receiver.Bind());
  fake_db_->UpdateCallback(true);
  ASSERT_TRUE(receiver.GetResult());
  EXPECT_TRUE(*receiver.GetResult());

  ASSERT_EQ(1ul, db_entries_.size());
  EXPECT_EQ(0, db_entries_["a/0"].local_action().id());

  receiver.GetResult().reset();
  store_->WriteActions({action}, receiver.Bind());
  fake_db_->UpdateCallback(false);
  EXPECT_NE(receiver.GetResult(), std::nullopt);
  EXPECT_EQ(receiver.GetResult().value(), false);
}

TEST_F(FeedStoreTest, RemoveActions) {
  MakeFeedStore({{"a/0", RecordForAction(MakeAction(0))},
                 {"a/1", RecordForAction(MakeAction(1))},
                 {"a/2", RecordForAction(MakeAction(2))}});

  const std::vector<LocalActionId> ids = {LocalActionId(0), LocalActionId(1),
                                          LocalActionId(2)};

  CallbackReceiver<bool> receiver;
  store_->RemoveActions(ids, receiver.Bind());
  fake_db_->UpdateCallback(true);
  EXPECT_EQ(receiver.GetResult().value(), true);
  EXPECT_EQ(db_entries_.size(), 0ul);

  receiver.GetResult().reset();
  store_->RemoveActions(ids, receiver.Bind());
  fake_db_->UpdateCallback(false);
  EXPECT_NE(receiver.GetResult(), std::nullopt);
  EXPECT_EQ(receiver.GetResult().value(), false);
}

TEST_F(FeedStoreTest, ClearAllSuccess) {
  // Write at least one record of each type.
  MakeFeedStore({});
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);
  store_->WriteActions({MakeAction(0)}, base::DoNothing());
  fake_db_->UpdateCallback(true);
  ASSERT_NE("", StoreToString());

  // ClearAll() and verify the DB is empty.
  CallbackReceiver<bool> receiver;
  store_->ClearAll(receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_TRUE(*receiver.GetResult());
  EXPECT_EQ("", StoreToString());
}

TEST_F(FeedStoreTest, ClearAllFail) {
  // Just verify that we can handle a storage failure. Note that |FakeDB| will
  // actually perform operations even when UpdateCallback(false) is called.
  MakeFeedStore({});

  CallbackReceiver<bool> receiver;
  store_->ClearAll(receiver.Bind());
  fake_db_->UpdateCallback(false);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_FALSE(*receiver.GetResult());
}

TEST_F(FeedStoreTest, ReadMetadata) {
  feedstore::Record record;
  record.mutable_metadata()->set_consistency_token("token");
  record.mutable_metadata()->set_next_action_id(20);
  MakeFeedStore({{"m", record}});

  CallbackReceiver<std::unique_ptr<feedstore::Metadata>> cr;
  store_->ReadMetadata(cr.Bind());
  fake_db_->GetCallback(true);
  ASSERT_TRUE(cr.GetResult());

  std::unique_ptr<feedstore::Metadata> metadata = std::move(*cr.GetResult());
  ASSERT_TRUE(metadata);
  EXPECT_EQ("token", metadata->consistency_token());
  EXPECT_EQ(20, metadata->next_action_id());

  store_->ReadMetadata(cr.Bind());
  fake_db_->GetCallback(false);
  ASSERT_TRUE(cr.GetResult());
  EXPECT_FALSE(*cr.GetResult());
}

TEST_F(FeedStoreTest, WriteMetadata) {
  MakeFeedStore({});

  feedstore::Metadata metadata;
  metadata.set_consistency_token("token");
  metadata.set_next_action_id(20);

  CallbackReceiver<bool> cr;
  store_->WriteMetadata(metadata, cr.Bind());
  fake_db_->UpdateCallback(true);
  ASSERT_TRUE(cr.GetResult());
  EXPECT_TRUE(*cr.GetResult());

  ASSERT_EQ(1ul, db_entries_.size());
  EXPECT_EQ("token", db_entries_["m"].metadata().consistency_token());
  EXPECT_EQ(20, db_entries_["m"].metadata().next_action_id());
}

TEST_F(FeedStoreTest, UpgradeFromStreamSchemaV0) {
  MakeFeedStore({});
  // Insert some junk with version 0 keys. It should be removed.
  db_entries_["S/0"].mutable_local_action()->set_id(6);
  db_entries_["T/0/0"].mutable_local_action()->set_id(6);
  db_entries_["T/0/73"].mutable_local_action()->set_id(6);
  db_entries_["c/stories,4,0"].mutable_local_action()->set_id(6);
  db_entries_["c/stories,4,1"].mutable_local_action()->set_id(6);
  db_entries_["c/garbage"].mutable_local_action()->set_id(6);
  db_entries_["s/render_data,0,0"].mutable_local_action()->set_id(6);
  db_entries_["s/garbage,0,0"].mutable_local_action()->set_id(6);
  // Actions should be retained.
  db_entries_["a/someaction"].mutable_local_action()->set_id(6);

  CallbackReceiver<feedstore::Metadata> receiver;
  feedstore::Metadata old_metadata;
  old_metadata.set_consistency_token("token-1");
  store_->UpgradeFromStreamSchemaV0(old_metadata, receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ("token-1", receiver.GetResult()->consistency_token());
  EXPECT_EQ(1, receiver.GetResult()->stream_schema_version());
  ASSERT_EQ(std::set<std::string>({
                "m",
                "a/someaction",
            }),
            StoredKeys());
}

TEST_F(FeedStoreTest, WriteRecommendedFeedsAndReadThem) {
  MakeFeedStore({});

  CallbackReceiver<> receiver;
  feedstore::RecommendedWebFeedIndex index;
  index.add_entries()->set_web_feed_id("foo");
  *index.mutable_entries(0)->add_matchers() =
      MakeWebFeedInfo("foo").matchers(0);
  index.add_entries()->set_web_feed_id("bar");
  *index.mutable_entries(1)->add_matchers() =
      MakeWebFeedInfo("bar").matchers(0);

  store_->WriteRecommendedFeeds(
      index, {MakeWebFeedInfo("foo"), MakeWebFeedInfo("bar")}, receiver.Bind());

  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.called());

  CallbackReceiver<FeedStore::WebFeedStartupData> startup_callback;
  store_->ReadWebFeedStartupData(startup_callback.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(startup_callback.GetResult());

  // Check that we can load the stored data.
  std::string want = R"({
  entries {
    matchers {
      criteria {
        text: "foo.com"
        criteria_type: 2
      }
    }
    web_feed_id: "foo"
  }
  entries {
    matchers {
      criteria {
        text: "bar.com"
        criteria_type: 2
      }
    }
    web_feed_id: "bar"
  }
}
)";
  EXPECT_STRINGS_EQUAL(
      want, ToTextProto(startup_callback.GetResult()->recommended_feed_index));

  CallbackReceiver<std::unique_ptr<feedstore::WebFeedInfo>> foo_callback;
  store_->ReadRecommendedWebFeedInfo("id_foo", foo_callback.Bind());
  fake_db_->GetCallback(true);
  ASSERT_TRUE(foo_callback.GetResult());
  ASSERT_TRUE(*foo_callback.GetResult());
  EXPECT_STRINGS_EQUAL(R"({
  web_feed_id: "id_foo"
  title: "Title foo"
  visit_uri: "https://foo.com"
  favicon {
    url: "http://favicon/foo"
  }
  follower_count: 123
  matchers {
    criteria {
      text: "foo.com"
      criteria_type: 2
    }
  }
}
)",
                       ToTextProto(**foo_callback.GetResult()));

  CallbackReceiver<std::unique_ptr<feedstore::WebFeedInfo>> bar_callback;
  store_->ReadRecommendedWebFeedInfo("id_bar", bar_callback.Bind());
  fake_db_->GetCallback(true);
  ASSERT_TRUE(bar_callback.GetResult());
  ASSERT_TRUE(*bar_callback.GetResult());
  EXPECT_STRINGS_EQUAL(R"({
  web_feed_id: "id_bar"
  title: "Title bar"
  visit_uri: "https://bar.com"
  favicon {
    url: "http://favicon/bar"
  }
  follower_count: 123
  matchers {
    criteria {
      text: "bar.com"
      criteria_type: 2
    }
  }
}
)",
                       ToTextProto(**bar_callback.GetResult()));
}

TEST_F(FeedStoreTest, WriteSubscribedFeeds) {
  MakeFeedStore({});

  CallbackReceiver<> receiver;
  feedstore::SubscribedWebFeeds subscribed_web_feeds;
  *subscribed_web_feeds.add_feeds() = MakeWebFeedInfo("foo");
  *subscribed_web_feeds.add_feeds() = MakeWebFeedInfo("bar");

  store_->WriteSubscribedFeeds(subscribed_web_feeds, receiver.Bind());

  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.called());

  CallbackReceiver<FeedStore::WebFeedStartupData> startup_callback;
  store_->ReadWebFeedStartupData(startup_callback.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(startup_callback.called());

  std::string want = R"({
  feeds {
    web_feed_id: "id_foo"
    title: "Title foo"
    visit_uri: "https://foo.com"
    favicon {
      url: "http://favicon/foo"
    }
    follower_count: 123
    matchers {
      criteria {
        text: "foo.com"
        criteria_type: 2
      }
    }
  }
  feeds {
    web_feed_id: "id_bar"
    title: "Title bar"
    visit_uri: "https://bar.com"
    favicon {
      url: "http://favicon/bar"
    }
    follower_count: 123
    matchers {
      criteria {
        text: "bar.com"
        criteria_type: 2
      }
    }
  }
}
)";
  EXPECT_STRINGS_EQUAL(
      want, ToTextProto(startup_callback.GetResult()->subscribed_web_feeds));
}

TEST_F(FeedStoreTest, ReadWebFeedStartupDataNotPresent) {
  MakeFeedStore({});

  CallbackReceiver<FeedStore::WebFeedStartupData> startup_callback;
  store_->ReadWebFeedStartupData(startup_callback.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(startup_callback.called());

  EXPECT_STRINGS_EQUAL(
      "{\n}\n",
      ToTextProto(startup_callback.GetResult()->subscribed_web_feeds));
  EXPECT_STRINGS_EQUAL(
      "{\n}\n",
      ToTextProto(startup_callback.GetResult()->recommended_feed_index));
}

TEST_F(FeedStoreTest, ReadRecommendedWebFeedInfoNotPresent) {
  MakeFeedStore({});

  CallbackReceiver<std::unique_ptr<feedstore::WebFeedInfo>> callback;
  store_->ReadRecommendedWebFeedInfo("id_foo", callback.Bind());
  fake_db_->GetCallback(true);

  ASSERT_TRUE(callback.GetResult());
  ASSERT_FALSE(*callback.GetResult());
}

TEST_F(FeedStoreTest, ClearAllStreamData) {
  // Write stream records to store.
  MakeFeedStore({});
  store_->OverwriteStream(StreamType(StreamKind::kSingleWebFeed, "A"),
                          MakeTypicalInitialModelState(), base::DoNothing());
  fake_db_->UpdateCallback(true);
  ASSERT_NE("", StoreToString());

  // ClearAll() and verify the DB is empty.
  CallbackReceiver<bool> receiver;
  store_->ClearAllStreamData(StreamKind::kSingleWebFeed, receiver.Bind());
  fake_db_->UpdateCallback(true);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_TRUE(*receiver.GetResult());
  EXPECT_EQ("", StoreToString());
}

TEST_F(FeedStoreTest, WriteDocView) {
  MakeFeedStore({});
  feedstore::DocView dv = CreateDocView(10, 11);
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);

  EXPECT_EQ(R"([v/10/11] {
  doc_view {
    docid: 10
    view_time_millis: 11
  }
}
)",
            StoreToString());
}

TEST_F(FeedStoreTest, RemoveDocViewsNotExist) {
  MakeFeedStore({});
  feedstore::DocView dv = CreateDocView(10, 11);
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);

  // docid doesn't match
  store_->RemoveDocViews({CreateDocView(11, 11)});
  fake_db_->UpdateCallback(true);

  EXPECT_EQ(R"([v/10/11] {
  doc_view {
    docid: 10
    view_time_millis: 11
  }
}
)",
            StoreToString());
}

TEST_F(FeedStoreTest, RemoveDocViewsDoesExist) {
  MakeFeedStore({});
  feedstore::DocView dv = CreateDocView(10, 9000);
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);
  dv.set_docid(11);
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);
  dv.set_docid(12);
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);

  store_->RemoveDocViews({CreateDocView(10, 9000), CreateDocView(12, 9000)});
  fake_db_->UpdateCallback(true);
  ASSERT_THAT(db_entries_, ElementsAre(Pair("v/11/9000", EqualsTextProto(R"({
  doc_view {
    docid: 11
    view_time_millis: 9000
  }
})"))));
}

TEST_F(FeedStoreTest, ReadDocViews) {
  MakeFeedStore({});
  feedstore::DocView dv;
  dv.set_docid(0);
  dv.set_view_time_millis(11);
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);
  dv.set_docid(std::numeric_limits<uint64_t>::max());
  store_->WriteDocView(dv);
  fake_db_->UpdateCallback(true);

  CallbackReceiver<std::vector<feedstore::DocView>> result;
  store_->ReadDocViews(result.Bind());
  fake_db_->LoadCallback(true);

  ASSERT_TRUE(result.GetResult());
  ASSERT_THAT(*result.GetResult(), ElementsAre(EqualsTextProto(R"({
  view_time_millis: 11
})"),
                                               EqualsTextProto(R"({
  docid: 18446744073709551615
  view_time_millis: 11
})")));
}

}  // namespace feed

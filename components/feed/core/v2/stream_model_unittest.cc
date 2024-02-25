// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using UiUpdate = StreamModel::UiUpdate;
using StoreUpdate = StreamModel::StoreUpdate;

std::vector<std::string> GetContentFrames(const StreamModel& model) {
  std::vector<std::string> frames;
  for (ContentRevision rev : model.GetContentList()) {
    const feedstore::Content* content = model.FindContent(rev);
    if (content) {
      frames.push_back(content->frame());
    } else {
      frames.push_back("<null>");
    }
  }
  return frames;
}

class TestObserver : public StreamModel::Observer {
 public:
  explicit TestObserver(StreamModel* model) { model->AddObserver(this); }

  // StreamModel::Observer.
  void OnUiUpdate(const UiUpdate& update) override { update_ = update; }
  const std::optional<UiUpdate>& GetUiUpdate() const { return update_; }
  bool ContentListChanged() const {
    return update_ && update_->content_list_changed;
  }

  void Clear() { update_ = std::nullopt; }

 private:
  std::optional<UiUpdate> update_;
};

class TestStoreObserver : public StreamModel::StoreObserver {
 public:
  explicit TestStoreObserver(StreamModel* model) {
    model->SetStoreObserver(this);
  }

  // StreamModel::StoreObserver.
  void OnStoreChange(StoreUpdate records) override {
    update_ = std::move(records);
  }

  const std::optional<StoreUpdate>& GetUpdate() const { return update_; }

  void Clear() { update_ = std::nullopt; }

 private:
  std::optional<StoreUpdate> update_;
};

TEST(StreamModelTest, ConstructEmptyModel) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  EXPECT_EQ(0UL, model.GetContentList().size());
}

TEST(StreamModelTest, ExecuteOperationsTypicalStream) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  TestStoreObserver store_observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
  ASSERT_TRUE(store_observer.GetUpdate());
  ASSERT_EQ(MakeTypicalStreamOperations().size(),
            store_observer.GetUpdate()->operations.size());
}

TEST(StreamModelTest, AddContentWithoutRoot) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeCluster(0, MakeRootId())),
      MakeOperation(MakeContentNode(0, MakeClusterId(0))),
      MakeOperation(MakeContent(0)),
  };
  model.ExecuteOperations(operations);

  // Without a root, no content is visible.
  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

// Verify Stream -> Content works.
TEST(StreamModelTest, AddStreamContent) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode(0, MakeRootId())),
      MakeOperation(MakeContent(0)),
  };
  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0"}), GetContentFrames(model));
}

TEST(StreamModelTest, AddRootAsChild) {
  // When the root is added as a child, it's no longer considered a root.
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  feedstore::StreamStructure stream_with_parent = MakeStream();
  *stream_with_parent.mutable_parent_id() = MakeContentContentId(0);
  std::vector<feedstore::DataOperation> operations{
      MakeOperation(MakeStream()),
      MakeOperation(MakeContentNode(0, MakeRootId())),
      MakeOperation(MakeContent(0)),
      MakeOperation(stream_with_parent),
  };

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveCluster) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId(0))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveContent) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeContentContentId(0))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveRoot) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeRootId())));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>(), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndAddRoot) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeRootId())));
  operations.push_back(MakeOperation(MakeStream()));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, SecondRootStreamIsIgnored) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  // Add a second stream root, but it is ignored.
  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  feedstore::StreamStructure root2 = MakeStream(2);
  root2.set_is_root(false);
  operations.push_back(MakeOperation(root2));
  operations.push_back(MakeOperation(MakeContentNode(9, MakeRootId(2))));
  operations.push_back(MakeOperation(MakeContent(9)));
  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));

  // Remove the first stream root, now the second root is used.
  model.ExecuteOperations({MakeOperation(MakeRemove(MakeRootId()))});

  EXPECT_EQ(std::vector<std::string>({"f:9"}), GetContentFrames(model));
}

TEST(StreamModelTest, SecondRootWithIsRootIsSelected) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  // Set up operations which add two roots. The second root is chosen because it
  // has is_root=true set.
  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations[0].mutable_structure()->set_is_root(false);
  operations.push_back(MakeOperation(MakeStream(2)));
  operations.push_back(MakeOperation(MakeContentNode(9, MakeRootId(2))));
  operations.push_back(MakeOperation(MakeContent(9)));
  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:9"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndUpdateCluster) {
  // Remove a cluster and add it back. Adding it back keeps its original
  // placement.
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId(0))));
  operations.push_back(MakeOperation(MakeCluster(0, MakeRootId())));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RemoveAndAppendToNewParent) {
  // Attempt to re-parent a node. This is not allowed, the old parent remains.
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  std::vector<feedstore::DataOperation> operations =
      MakeTypicalStreamOperations();
  operations.push_back(MakeOperation(MakeRemove(MakeClusterId(0))));
  operations.push_back(MakeOperation(MakeCluster(0, MakeClusterId(1))));

  model.ExecuteOperations(operations);

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, EphemeralNewCluster) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  observer.Clear();

  model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1", "f:2"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, CommitEphemeralChange) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());

  EphemeralChangeId change_id = model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  observer.Clear();
  TestStoreObserver store_observer(&model);
  EXPECT_TRUE(model.CommitEphemeralChange(change_id));

  // Check that the observer's |OnStoreChange()| was called.
  ASSERT_TRUE(store_observer.GetUpdate());
  const StoreUpdate& store_update = *store_observer.GetUpdate();
  ASSERT_EQ(3UL, store_update.operations.size());
  EXPECT_EQ(feedstore::StreamStructure::GROUP,
            store_update.operations[0].structure().type());
  EXPECT_EQ(feedstore::StreamStructure::CONTENT,
            store_update.operations[1].structure().type());

  // Can't reject after commit.
  EXPECT_FALSE(model.RejectEphemeralChange(change_id));

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1", "f:2"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, RejectEphemeralChange) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id = model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });
  observer.Clear();

  EXPECT_TRUE(model.RejectEphemeralChange(change_id));
  EXPECT_TRUE(observer.ContentListChanged());
  // Can't commit after reject.
  EXPECT_FALSE(model.CommitEphemeralChange(change_id));

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
}

TEST(StreamModelTest, RejectFirstEphemeralChange) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);

  model.ExecuteOperations(MakeTypicalStreamOperations());
  EphemeralChangeId change_id1 = model.CreateEphemeralChange({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  model.CreateEphemeralChange({
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  });
  observer.Clear();

  EXPECT_TRUE(model.RejectEphemeralChange(change_id1));
  EXPECT_TRUE(observer.ContentListChanged());
  // Can't commit after reject.
  EXPECT_FALSE(model.CommitEphemeralChange(change_id1));

  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1", "f:3"}),
            GetContentFrames(model));
}

TEST(StreamModelTest, InitialLoad) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  TestStoreObserver store_observer(&model);
  model.Update(MakeTypicalInitialModelState());

  // Check that content was added and the store doesn't receive its own update.
  EXPECT_TRUE(observer.ContentListChanged());
  EXPECT_EQ(std::vector<std::string>({"f:0", "f:1"}), GetContentFrames(model));
  ASSERT_EQ(1UL, observer.GetUiUpdate()->shared_states.size());
  EXPECT_NE("", observer.GetUiUpdate()->shared_states[0].shared_state_id);
  const std::string* shared_state_data = model.FindSharedStateData(
      observer.GetUiUpdate()->shared_states[0].shared_state_id);
  ASSERT_TRUE(shared_state_data);
  EXPECT_EQ("ss:0", *shared_state_data);
  EXPECT_FALSE(store_observer.GetUpdate());
}

TEST(StreamModelTest, StoreObserverReceivesIncreasingSequenceNumbers) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  TestStoreObserver store_observer(&model);

  // Initialize the model starting at sequence number 5.
  {
    std::unique_ptr<StreamModelUpdateRequest> initial_state =
        MakeTypicalInitialModelState();
    initial_state->max_structure_sequence_number = 5;
    model.Update(std::move(initial_state));
  }

  model.ExecuteOperations({MakeOperation(MakeRemove(MakeContentContentId(0)))});

  ASSERT_TRUE(store_observer.GetUpdate());
  EXPECT_EQ(6, store_observer.GetUpdate()->sequence_number);

  store_observer.Clear();
  model.ExecuteOperations({MakeOperation(MakeRemove(MakeContentContentId(0)))});

  ASSERT_TRUE(store_observer.GetUpdate());
  EXPECT_EQ(7, store_observer.GetUpdate()->sequence_number);
}

TEST(StreamModelTest, SharedStateCanBeAddedOnlyOnce) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  TestStoreObserver store_observer(&model);

  // Update the model twice with this request. The shared state should not
  // be added the second time.
  StreamModelUpdateRequest update_request;
  update_request.source =
      StreamModelUpdateRequest::Source::kInitialLoadFromStore;
  update_request.content.push_back(MakeContent(0));
  update_request.stream_structures = {MakeStream(),
                                      MakeCluster(0, MakeRootId()),
                                      MakeContentNode(0, MakeClusterId(0))};
  update_request.shared_states.push_back(MakeSharedState(0));

  model.Update(std::make_unique<StreamModelUpdateRequest>(update_request));
  observer.Clear();
  model.Update(std::make_unique<StreamModelUpdateRequest>(update_request));
  ASSERT_EQ(1UL, observer.GetUiUpdate()->shared_states.size());
  EXPECT_FALSE(observer.GetUiUpdate()->shared_states[0].updated);
}

TEST(StreamModelTest, SharedStateUpdatesKeepOriginal) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  TestStoreObserver store_observer(&model);
  model.Update(MakeTypicalInitialModelState());
  observer.Clear();
  store_observer.Clear();
  model.Update(MakeTypicalNextPageState(
      2, kTestTimeEpoch, true, true, true,
      StreamModelUpdateRequest::Source::kNetworkLoadMore));

  EXPECT_EQ(2UL, observer.GetUiUpdate()->shared_states.size());
  EXPECT_FALSE(observer.GetUiUpdate()->shared_states[0].updated);
  EXPECT_TRUE(observer.GetUiUpdate()->shared_states[1].updated);

  ASSERT_TRUE(store_observer.GetUpdate());
  EXPECT_EQ(1, store_observer.GetUpdate()->sequence_number);
  ASSERT_EQ(2, store_observer.GetUpdate()
                   ->update_request->stream_data.shared_state_ids_size());
}

TEST(StreamModelTest, ClearAllErasesSharedStates) {
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters());
  TestObserver observer(&model);
  TestStoreObserver store_observer(&model);
  // CLEAR_ALL is the first operation in the typical initial model state.
  // The second Update() will therefore need to remove and add the shared
  // state.
  model.Update(MakeTypicalInitialModelState());
  observer.Clear();
  model.Update(MakeTypicalInitialModelState());

  ASSERT_EQ(1UL, observer.GetUiUpdate()->shared_states.size());
  EXPECT_TRUE(observer.GetUiUpdate()->shared_states[0].updated);
}

}  // namespace
}  // namespace feed

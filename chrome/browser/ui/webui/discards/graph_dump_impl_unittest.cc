// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/graph_dump_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/test_support/graph_impl.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using performance_manager::NodeBase;

const char kHtmlMimeType[] = "text/html";
const blink::mojom::PermissionStatus kAskPermissionStatus =
    blink::mojom::PermissionStatus::ASK;

class TestChangeStream : public discards::mojom::GraphChangeStream {
 public:
  using FrameMap = std::map<int64_t, discards::mojom::FrameInfoPtr>;
  using PageMap = std::map<int64_t, discards::mojom::PageInfoPtr>;
  using ProcessMap = std::map<int64_t, discards::mojom::ProcessInfoPtr>;
  using WorkerMap = std::map<int64_t, discards::mojom::WorkerInfoPtr>;
  using IdSet = std::set<int64_t>;

  TestChangeStream() {}

  mojo::PendingRemote<discards::mojom::GraphChangeStream> GetRemote() {
    mojo::PendingRemote<discards::mojom::GraphChangeStream> remote;

    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());

    return remote;
  }

  // discards::mojom::GraphChangeStream implementation
  void FrameCreated(discards::mojom::FrameInfoPtr frame) override {
    EXPECT_FALSE(HasId(frame->id));
    // If the node has a parent frame, we must have heard of it.
    EXPECT_TRUE(HasIdIfValid(frame->parent_frame_id));
    EXPECT_TRUE(HasId(frame->page_id));
    EXPECT_TRUE(HasId(frame->process_id));

    id_set_.insert(frame->id);
    frame_map_.insert(std::make_pair(frame->id, std::move(frame)));
  }

  void PageCreated(discards::mojom::PageInfoPtr page) override {
    EXPECT_FALSE(HasId(page->id));
    id_set_.insert(page->id);
    page_map_.insert(std::make_pair(page->id, std::move(page)));
  }

  void ProcessCreated(discards::mojom::ProcessInfoPtr process) override {
    EXPECT_FALSE(HasId(process->id));
    id_set_.insert(process->id);
    process_map_.insert(std::make_pair(process->id, std::move(process)));
  }

  void WorkerCreated(discards::mojom::WorkerInfoPtr worker) override {
    EXPECT_FALSE(HasId(worker->id));
    id_set_.insert(worker->id);
    worker_map_.insert(std::make_pair(worker->id, std::move(worker)));
  }

  void FrameChanged(discards::mojom::FrameInfoPtr frame) override {
    EXPECT_TRUE(HasId(frame->id));
    frame_map_[frame->id] = std::move(frame);
    ++num_changes_;
  }

  void PageChanged(discards::mojom::PageInfoPtr page) override {
    EXPECT_TRUE(HasId(page->id));
    page_map_[page->id] = std::move(page);
    ++num_changes_;
  }

  void ProcessChanged(discards::mojom::ProcessInfoPtr process) override {
    EXPECT_TRUE(HasId(process->id));
    process_map_[process->id] = std::move(process);
    ++num_changes_;
  }

  void WorkerChanged(discards::mojom::WorkerInfoPtr worker) override {
    EXPECT_TRUE(HasId(worker->id));
    worker_map_[worker->id] = std::move(worker);
    ++num_changes_;
  }

  void FavIconDataAvailable(discards::mojom::FavIconInfoPtr favicon) override {}

  void NodeDeleted(int64_t node_id) override {
    EXPECT_EQ(1u, id_set_.erase(node_id));

    size_t erased = frame_map_.erase(node_id) + page_map_.erase(node_id) +
                    process_map_.erase(node_id);
    EXPECT_EQ(1u, erased);
  }

  const FrameMap& frame_map() const { return frame_map_; }
  const PageMap& page_map() const { return page_map_; }
  const ProcessMap& process_map() const { return process_map_; }
  const WorkerMap& worker_map() const { return worker_map_; }
  const IdSet& id_set() const { return id_set_; }
  size_t num_changes() const { return num_changes_; }

 private:
  bool HasId(int64_t id) { return base::Contains(id_set_, id); }
  bool HasIdIfValid(int64_t id) { return id == 0u || HasId(id); }

  FrameMap frame_map_;
  PageMap page_map_;
  ProcessMap process_map_;
  WorkerMap worker_map_;
  IdSet id_set_;
  size_t num_changes_ = 0;

  mojo::Receiver<discards::mojom::GraphChangeStream> receiver_{this};
};

class DiscardsGraphDumpImplTest : public testing::Test {
 public:
  void SetUp() override { graph_.SetUp(); }
  void TearDown() override { graph_.TearDown(); }

 protected:
  performance_manager::TestGraphImpl graph_;
};

class TestNodeDataDescriber : public performance_manager::NodeDataDescriber {
 public:
  // NodeDataDescriber implementations:
  base::Value::Dict DescribeFrameNodeData(
      const performance_manager::FrameNode* node) const override {
    base::Value::Dict dict;
    dict.Set("type", "frame");
    return dict;
  }
  base::Value::Dict DescribePageNodeData(
      const performance_manager::PageNode* node) const override {
    base::Value::Dict dict;
    dict.Set("type", "page");
    return dict;
  }
  base::Value::Dict DescribeProcessNodeData(
      const performance_manager::ProcessNode* node) const override {
    base::Value::Dict dict;
    dict.Set("type", "process");
    return dict;
  }
  base::Value::Dict DescribeSystemNodeData(
      const performance_manager::SystemNode* node) const override {
    base::Value::Dict dict;
    dict.Set("type", "system");
    return dict;
  }
  base::Value::Dict DescribeWorkerNodeData(
      const performance_manager::WorkerNode* node) const override {
    base::Value::Dict dict;
    dict.Set("type", "worker");
    return dict;
  }
};

}  // namespace

TEST_F(DiscardsGraphDumpImplTest, ChangeStream) {
  using performance_manager::TestNodeWrapper;
  using performance_manager::WorkerNodeImpl;
  content::BrowserTaskEnvironment task_environment;

  performance_manager::MockMultiplePagesWithMultipleProcessesGraph mock_graph(
      &graph_);
  TestNodeWrapper<WorkerNodeImpl> worker(
      TestNodeWrapper<WorkerNodeImpl>::Create(
          &graph_, performance_manager::WorkerNode::WorkerType::kDedicated,
          mock_graph.process.get()));

  worker->AddClientFrame(mock_graph.frame.get());

  base::TimeTicks now = base::TimeTicks::Now();

  const GURL kExampleUrl("http://www.example.org");
  int64_t next_navigation_id = 1;
  mock_graph.page->OnMainFrameNavigationCommitted(
      false, now, next_navigation_id++, kExampleUrl, kHtmlMimeType,
      kAskPermissionStatus);
  mock_graph.other_page->OnMainFrameNavigationCommitted(
      false, now, next_navigation_id++, kExampleUrl, kHtmlMimeType,
      kAskPermissionStatus);

  auto* main_frame = mock_graph.page->main_frame_node();
  main_frame->OnNavigationCommitted(
      kExampleUrl, url::Origin::Create(kExampleUrl), /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);

  std::unique_ptr<DiscardsGraphDumpImpl> impl =
      std::make_unique<DiscardsGraphDumpImpl>();
  DiscardsGraphDumpImpl* impl_raw = impl.get();
  // Create a mojo remote to the impl.
  mojo::Remote<discards::mojom::GraphDump> graph_dump_remote;
  impl->BindWithGraph(&graph_, graph_dump_remote.BindNewPipeAndPassReceiver());
  graph_.PassToGraph(std::move(impl));

  TestNodeDataDescriber describer;
  graph_.GetNodeDataDescriberRegistry()->RegisterDescriber(&describer, "test");

  TestChangeStream change_stream;
  graph_dump_remote->SubscribeToChanges(change_stream.GetRemote());

  task_environment.RunUntilIdle();

  // Validate that the initial graph state dump is complete. Note that there is
  // an update for each node as part of the initial state dump, except the
  // system node.
  size_t expected_changes =
      graph_.GetAllFrameNodes().size() + graph_.GetAllPageNodes().size() +
      graph_.GetAllProcessNodes().size() + graph_.GetAllWorkerNodes().size();
  EXPECT_EQ(expected_changes, change_stream.num_changes());
  EXPECT_EQ(expected_changes, change_stream.id_set().size());

  EXPECT_EQ(graph_.GetAllProcessNodes().size(),
            change_stream.process_map().size());
  for (const auto& kv : change_stream.process_map()) {
    const auto* process_info = kv.second.get();
    EXPECT_NE(0u, process_info->id);
    EXPECT_EQ(base::JSONReader::Read("{\"test\":{\"type\":\"process\"}}"),
              base::JSONReader::Read(process_info->description_json));
  }

  EXPECT_EQ(graph_.GetAllFrameNodes().size(), change_stream.frame_map().size());
  for (const auto& kv : change_stream.frame_map()) {
    EXPECT_EQ(base::JSONReader::Read("{\"test\":{\"type\":\"frame\"}}"),
              base::JSONReader::Read(kv.second->description_json));
  }
  EXPECT_EQ(graph_.GetAllWorkerNodes().size(),
            change_stream.worker_map().size());
  for (const auto& kv : change_stream.worker_map()) {
    EXPECT_EQ(base::JSONReader::Read("{\"test\":{\"type\":\"worker\"}}"),
              base::JSONReader::Read(kv.second->description_json));
  }

  // Count the top-level frames as we go.
  size_t top_level_frames = 0;
  for (const auto& kv : change_stream.frame_map()) {
    const auto& frame = kv.second;
    if (frame->parent_frame_id == 0) {
      ++top_level_frames;

      // Top level frames should have a page ID.
      EXPECT_NE(0u, frame->page_id);

      // The page's main frame should have an URL.
      if (frame->id == impl_raw->GetNodeIdForTesting(main_frame))
        EXPECT_EQ(kExampleUrl, frame->url);
    }
    EXPECT_NE(0u, frame->id);
    EXPECT_NE(0u, frame->process_id);
  }

  // Make sure we have one top-level frame per page.
  EXPECT_EQ(change_stream.page_map().size(), top_level_frames);

  EXPECT_EQ(graph_.GetAllPageNodes().size(), change_stream.page_map().size());
  for (const auto& kv : change_stream.page_map()) {
    const auto& page = kv.second;
    EXPECT_NE(0u, page->id);
    EXPECT_EQ(kExampleUrl, page->main_frame_url);
    EXPECT_EQ(base::JSONReader::Read("{\"test\":{\"type\":\"page\"}}"),
              base::JSONReader::Read(kv.second->description_json));
  }

  // Test change notifications.
  const GURL kAnotherURL("http://www.google.com/");
  mock_graph.page->OnMainFrameNavigationCommitted(
      false, now, next_navigation_id++, kAnotherURL, kHtmlMimeType,
      kAskPermissionStatus);

  size_t child_frame_id =
      impl_raw->GetNodeIdForTesting(mock_graph.child_frame.get());
  mock_graph.child_frame.reset();

  task_environment.RunUntilIdle();

  // Main frame navigation results in a notification for the url.
  expected_changes += 1;
  EXPECT_EQ(expected_changes, change_stream.num_changes());
  EXPECT_FALSE(base::Contains(change_stream.id_set(), child_frame_id));

  const auto main_page_it = change_stream.page_map().find(
      impl_raw->GetNodeIdForTesting(mock_graph.page.get()));
  ASSERT_TRUE(main_page_it != change_stream.page_map().end());
  EXPECT_EQ(kAnotherURL, main_page_it->second->main_frame_url);

  task_environment.RunUntilIdle();

  // Test RequestNodeDescriptions.
  std::vector<int64_t> descriptions_requested;
  for (int64_t node_id : change_stream.id_set()) {
    descriptions_requested.push_back(node_id);
  }
  // Increase the last ID by one. As the entries are in increasing order, this
  // results in a request for all but one nodes, and one non-existent node id.
  descriptions_requested.back() += 1;

  {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();

    graph_dump_remote->RequestNodeDescriptions(
        descriptions_requested,
        base::BindLambdaForTesting(
            [&descriptions_requested,
             &quit_closure](const base::flat_map<int64_t, std::string>&
                                node_descriptions_json) {
              std::vector<int64_t> keys_received;
              // Check that the descriptions make sense.
              for (auto kv : node_descriptions_json) {
                keys_received.push_back(kv.first);
                std::optional<base::Value> v =
                    base::JSONReader::Read(kv.second);
                EXPECT_TRUE(v->is_dict());
                base::Value::Dict* dict = v->GetDict().FindDict("test");
                EXPECT_TRUE(dict);
                std::string* str = dict->FindString("type");
                EXPECT_TRUE(str);
                if (str) {
                  EXPECT_TRUE(*str == "frame" || *str == "page" ||
                              *str == "process" || *str == "worker");
                }
              }

              EXPECT_THAT(keys_received,
                          ::testing::UnorderedElementsAreArray(
                              descriptions_requested.data(),
                              descriptions_requested.size() - 1));

              quit_closure.Run();
            }));

    run_loop.Run();
  }

  task_environment.RunUntilIdle();

  // Make sure the Dump impl is torn down when the proxy closes.
  graph_dump_remote.reset();
  task_environment.RunUntilIdle();

  EXPECT_EQ(nullptr, graph_.TakeFromGraph(impl_raw));

  worker->RemoveClientFrame(mock_graph.frame.get());

  graph_.GetNodeDataDescriberRegistry()->UnregisterDescriber(&describer);
}

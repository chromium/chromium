// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "base/containers/contains.h"
#include "base/test/task_environment.h"
#include "components/services/screen_ai/screen_ai_library_wrapper.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace screen_ai {

namespace {
void MockLibraryInitializeResultFunction(bool result) {}

bool MockExtractMainContentFunction(
    const char* /*serialized_view_hierarchy*/,
    uint32_t /*serialized_view_hierarchy_length*/,
    uint32_t& content_node_ids_length) {
  content_node_ids_length = 10;
  return true;
}

bool MockReadBufferedInt32ArrayFunction(int32_t* /*results*/,
                                        uint32_t /*max_count*/) {
  return true;
}

ui::AXTreeUpdate CreateAXTreeUpdateSample() {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kDialog;
  root.AddState(ax::mojom::State::kFocusable);
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  ui::AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;
  button.relative_bounds.bounds = gfx::RectF(20, 20, 200, 30);

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.relative_bounds.bounds = gfx::RectF(20, 50, 200, 30);

  ui::AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root);
  initial_state.nodes.push_back(button);
  initial_state.nodes.push_back(checkbox);
  initial_state.has_tree_data = true;
  initial_state.tree_data.title = "Title";
  ui::AXSerializableTree src_tree(initial_state);

  std::unique_ptr<ui::AXTreeSource<const ui::AXNode*>> tree_source(
      src_tree.CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*> serializer(tree_source.get());
  ui::AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.root(), &update);
  return update;
}

class MockScreenAIService : public ScreenAIService {
 public:
  // The service is initialized with an empty mojo receiver as mojo
  // functionality is not tested here.
  MockScreenAIService()
      : ScreenAIService(mojo::PendingReceiver<mojom::ScreenAIService>()) {}

  mojo::ReceiverId GetMainContentExtractorReceiverId() override { return 0; }
};

}  // namespace

class ScreenAIServiceImplTest : public ::testing::Test {
 public:
  void StartProcessingQueuedRequests() {
    std::unique_ptr<ScreenAILibraryWrapper> library =
        std::make_unique<ScreenAILibraryWrapper>();
    library->extract_main_content_ = &MockExtractMainContentFunction;
    library->read_buffered_int32_array_ = &MockReadBufferedInt32ArrayFunction;
    service_.SetLibraryAndStartProcessingQueuedTasks(
        base::BindOnce(&MockLibraryInitializeResultFunction),
        std::move(library));
  }

  void RequestMainContentExtraction(const std::string& task_name,
                                    const ui::AXTreeUpdate& snapshot) {
    service_.ExtractMainContent(
        snapshot, ukm::kInvalidSourceId,
        base::BindOnce(&ScreenAIServiceImplTest::MainContentCallBack,
                       base::Unretained(this), task_name));
  }

  void CancelPendingMainContentExtractionTasks() {
    service_.CancelPendingMainContentExtractionTasks();
  }

  void WaitForAllTasksCompletion() { task_environment_.RunUntilIdle(); }

  mojom::Screen2xMainContentExtractor::Status GetContentExtractionTaskResult(
      const std::string& task_name) {
    EXPECT_TRUE(base::Contains(content_extraction_results_, task_name));
    return content_extraction_results_[task_name];
  }

 private:
  void MainContentCallBack(const std::string task_name,
                           mojom::Screen2xMainContentExtractor::Status status,
                           const std::vector<int32_t>& nodes) {
    EXPECT_FALSE(base::Contains(content_extraction_results_, task_name));
    content_extraction_results_[task_name] = status;
  }

  base::test::TaskEnvironment task_environment_;
  MockScreenAIService service_;
  std::map<std::string, mojom::Screen2xMainContentExtractor::Status>
      content_extraction_results_;
};

// Tesk scheduling several tasks will results in all tasks being processed.
TEST_F(ScreenAIServiceImplTest, MainContentFullFlow) {
  std::vector<const std::string> tasks({"Task-1", "Task-2", "Task-3"});
  ui::AXTreeUpdate snapshot = CreateAXTreeUpdateSample();

  for (const std::string& task : tasks) {
    RequestMainContentExtraction(task, snapshot);
  }

  StartProcessingQueuedRequests();
  WaitForAllTasksCompletion();

  for (const std::string& task : tasks) {
    EXPECT_EQ(GetContentExtractionTaskResult(task),
              mojom::Screen2xMainContentExtractor::Status::kOK);
  }
}

// Test if canceling a request, before getting processed, will return 'Canceled'
// state.
TEST_F(ScreenAIServiceImplTest, MainContentWithCancellation) {
  ui::AXTreeUpdate snapshot = CreateAXTreeUpdateSample();

  RequestMainContentExtraction("Task-1", snapshot);
  CancelPendingMainContentExtractionTasks();
  RequestMainContentExtraction("Task-2", snapshot);

  StartProcessingQueuedRequests();
  WaitForAllTasksCompletion();

  EXPECT_EQ(GetContentExtractionTaskResult("Task-1"),
            mojom::Screen2xMainContentExtractor::Status::kCanceled);
  EXPECT_EQ(GetContentExtractionTaskResult("Task-2"),
            mojom::Screen2xMainContentExtractor::Status::kOK);
}

// Test if sending an empty snapshot returns gracefully.
TEST_F(ScreenAIServiceImplTest, MainContentWithEmptyInput) {
  RequestMainContentExtraction("Task-1", ui::AXTreeUpdate());

  StartProcessingQueuedRequests();
  WaitForAllTasksCompletion();

  EXPECT_EQ(GetContentExtractionTaskResult("Task-1"),
            mojom::Screen2xMainContentExtractor::Status::kOK);
}

TEST(ScreenAIServiceImplMetricsRecordingTest, Success) {
  ukm::TestUkmRecorder test_recorder;
  base::TimeDelta elapsed_time = base::Microseconds(1000);
  ScreenAIService::RecordMetrics(test_recorder.GetNewSourceID(), &test_recorder,
                                 elapsed_time, true);

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_ScreenAI::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries.front(),
                                  "Screen2xDistillationTime.Success", 1);
}

TEST(ScreenAIServiceImplMetricsRecordingTest, Failure) {
  ukm::TestUkmRecorder test_recorder;
  base::TimeDelta elapsed_time = base::Microseconds(2000);
  ScreenAIService::RecordMetrics(test_recorder.GetNewSourceID(), &test_recorder,
                                 elapsed_time, false);

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_ScreenAI::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries.front(),
                                  "Screen2xDistillationTime.Failure", 2);
}

TEST(ScreenAIServiceImplMetricsRecordingTest, InvalidSourceID) {
  ukm::TestUkmRecorder test_recorder;
  base::TimeDelta elapsed_time = base::Microseconds(1000);
  ScreenAIService::RecordMetrics(ukm::kInvalidSourceId, &test_recorder,
                                 elapsed_time, false);

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_ScreenAI::kEntryName);
  ASSERT_EQ(entries.size(), 0u);
}

}  // namespace screen_ai

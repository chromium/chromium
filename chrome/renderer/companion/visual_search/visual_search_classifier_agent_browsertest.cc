// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "chrome/common/companion/visual_search.mojom.h"
#include "chrome/renderer/companion/visual_search/visual_search_classifier_agent.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Return;

namespace companion::visual_search {

namespace {

base::File LoadModelFile(const base::FilePath& model_file_path) {
  if (!base::PathExists(model_file_path)) {
    return base::File();
  }

  return base::File(model_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("companion_visual_search")
      .AppendASCII("test-model-quantized.tflite");
}

base::FilePath img_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("companion_visual_search")
      .AppendASCII("base64_img.txt");
}

}  // namespace

class TestVisualResultHandler : mojom::VisualSuggestionsResultHandler {
 public:
  TestVisualResultHandler() = default;

  mojo::PendingRemote<mojom::VisualSuggestionsResultHandler>
  GetRemoteHandler() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD2(HandleClassification,
               void(std::vector<mojom::VisualSearchSuggestionPtr>,
                    mojom::ClassificationStatsPtr));

 private:
  mojo::Receiver<mojom::VisualSuggestionsResultHandler> receiver_{this};
};

class VisualSearchClassifierAgentTest : public ChromeRenderViewTest {
 public:
  VisualSearchClassifierAgentTest() = default;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    content::RenderFrame* render_frame = GetMainRenderFrame();
    render_frame->GetAssociatedInterfaceRegistry()->RemoveInterface(
        mojom::VisualSuggestionsRequestHandler::Name_);
    agent_ = VisualSearchClassifierAgent::Create(render_frame);
    model_file_ = LoadModelFile(model_file_path());
    base::DiscardableMemoryAllocator::SetInstance(&test_allocator_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
    // Simulate RenderFrame OnDestruct() call.
    agent_->OnDestruct();
    ChromeRenderViewTest::TearDown();
  }

  void LoadHtmlWithSingleImage() {
    base::FilePath img_path = img_file_path();
    std::string base64_img;
    ASSERT_TRUE(base::ReadFileToString(img_path, &base64_img));
    std::string html = "<html><body><img src=\"";
    html.append(base64_img);
    html.append("\"</body></html>");
    LoadHTML(html.c_str());
  }

 protected:
  VisualSearchClassifierAgent* agent_;  // Owned by RenderFrame
  base::HistogramTester histogram_tester_;
  TestVisualResultHandler test_handler_;
  base::File model_file_;
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_F(VisualSearchClassifierAgentTest,
       StartClassification_SingleImageNonShoppy) {
  LoadHtmlWithSingleImage();
  agent_->StartVisualClassification(model_file_.Duplicate(), "",
                                    test_handler_.GetRemoteHandler());
  base::RunLoop().RunUntilIdle();
  // TODO(b/287637476) - Remove the file valid check.
  // This validity check is needed because file path does not seem to work on
  // on certain platforms (i.e. linux-lacros-rel, linux-wayland).
  if (model_file_.IsValid()) {
    histogram_tester_.ExpectBucketCount(
        "Companion.VisualQuery.Agent.DomImageCount", 1, 1);
  }
}

TEST_F(VisualSearchClassifierAgentTest, StartClassification_NoImages) {
  std::string html = "<html><body>dummy</body></html>";
  LoadHTML(html.c_str());
  agent_->StartVisualClassification(model_file_.Duplicate(), "",
                                    test_handler_.GetRemoteHandler());
  base::RunLoop().RunUntilIdle();

  // We don't expect handler to get called since there are no images in DOM.
  EXPECT_CALL(test_handler_, HandleClassification(_, _)).Times(0);

  // TODO(b/287637476) - Remove the file valid check.
  // This validity check is needed because file path does not seem to work on
  // on certain platforms (i.e. linux-lacros-rel, linux-wayland).
  if (model_file_.IsValid()) {
    histogram_tester_.ExpectBucketCount(
        "Companion.VisualQuery.Agent.StartClassification", false, 1);
  }
}

TEST_F(VisualSearchClassifierAgentTest, StartClassification_InvalidModel) {
  base::File file;
  LoadHtmlWithSingleImage();
  agent_->StartVisualClassification(file.Duplicate(), "",
                                    test_handler_.GetRemoteHandler());
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(test_handler_, HandleClassification(_, _)).Times(0);
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualSearch.Agent.InvalidModelFailure", true, 1);
}
}  // namespace companion::visual_search

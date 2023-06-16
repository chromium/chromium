// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/renderer/companion/visual_search/visual_search_classifier_agent.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  return source_root_dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("visual_model.tflite");
}

}  // namespace

class VisualSearchClassifierAgentTest : public ChromeRenderViewTest {
 public:
  VisualSearchClassifierAgentTest() = default;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    agent_ = VisualSearchClassifierAgent::Create(GetMainRenderFrame());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Simulate RenderFrame OnDestruct() call.
    agent_->OnDestruct();
    ChromeRenderViewTest::TearDown();
  }

 protected:
  VisualSearchClassifierAgent* agent_;  // Owned by RenderFrame
  base::HistogramTester histogram_tester_;
};

TEST_F(VisualSearchClassifierAgentTest, StartClassification_NoImages) {
  base::File file = LoadModelFile(model_file_path());
  std::string html = "<html><body>dummy</body></html>";
  LoadHTML(html.c_str());
  base::RunLoop().RunUntilIdle();
  VisualSearchClassifierAgent::ClassifierResultCallback callback =
      base::BindOnce(
          [](std::vector<SkBitmap> results) { EXPECT_EQ(results.size(), 0U); });
  agent_->StartVisualClassification(file.Duplicate(), "", std::move(callback));
  base::RunLoop().RunUntilIdle();

  // TODO(b/287637476) - Remove the file valid check.
  // This validity check is needed because file path does not seem to work on
  // on certain platforms (i.e. linux-lacros-rel, linux-wayland).
  if (file.IsValid()) {
    histogram_tester_.ExpectBucketCount(
        "Companion.VisualSearch.Agent.DomImageCount", 0, 1);
  }
}

TEST_F(VisualSearchClassifierAgentTest, StartClassification_InvalidModel) {
  base::File file;
  std::string html = "<html><body>dummy</body></html>";
  LoadHTML(html.c_str());
  VisualSearchClassifierAgent::ClassifierResultCallback callback =
      base::BindOnce([](std::vector<SkBitmap> results) {});
  agent_->StartVisualClassification(file.Duplicate(), "", std::move(callback));
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualSearch.Agent.InvalidModelFailure", false, 1);
}
}  // namespace companion::visual_search

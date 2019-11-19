// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <sstream>

#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/transform_node.h"
#include "components/viz/service/display/bsp_tree.h"
#include "components/viz/service/display/draw_polygon.h"
#include "components/viz/test/paths.h"
#include "testing/perf/perf_result_reporter.h"

namespace viz {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

const char kMetricPrefixBspTree[] = "BspTree.";
const char kMetricCalcDrawPropsTimeUs[] = "calc_draw_props_time";

perf_test::PerfResultReporter SetUpBspTreeReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixBspTree, story);
  reporter.RegisterImportantMetric(kMetricCalcDrawPropsTimeUs, "us");
  return reporter;
}

class BspTreePerfTest : public cc::LayerTreeTest {
 public:
  BspTreePerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void SetupTree() override {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(viewport), 1.f,
                                               LocalSurfaceIdAllocation());
    scoped_refptr<cc::Layer> root =
        ParseTreeFromJson(json_, &content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree_host()->SetRootLayer(root);
    content_layer_client_.set_bounds(viewport);
  }

  void SetStory(const std::string& story) { story_ = story; }

  void SetNumberOfDuplicates(int num_duplicates) {
    num_duplicates_ = num_duplicates;
  }

  void ReadTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(Paths::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(base::ReadFileToString(json_file, &json_));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(cc::LayerTreeHostImpl* host_impl) override {
    cc::LayerTreeImpl* active_tree = host_impl->active_tree();
    // First build the tree and then we'll start running tests on layersorter
    // itself
    host_impl->active_tree()->UpdateDrawProperties();

    cc::LayerImplList base_list;
    BuildLayerImplList(active_tree->root_layer(), &base_list);

    int polygon_counter = 0;
    std::vector<std::unique_ptr<DrawPolygon>> polygon_list;
    for (auto it = base_list.begin(); it != base_list.end(); ++it) {
      DrawPolygon* draw_polygon = new DrawPolygon(
          nullptr, gfx::RectF(gfx::SizeF((*it)->bounds())),
          (*it)->draw_properties().target_space_transform, polygon_counter++);
      polygon_list.push_back(std::unique_ptr<DrawPolygon>(draw_polygon));
    }

    timer_.Reset();
    do {
      base::circular_deque<std::unique_ptr<DrawPolygon>> test_list;
      for (int i = 0; i < num_duplicates_; i++) {
        for (size_t i = 0; i < polygon_list.size(); i++) {
          test_list.push_back(polygon_list[i]->CreateCopy());
        }
      }
      BspTree bsp_tree(&test_list);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }

  void BuildLayerImplList(cc::LayerImpl* layer, cc::LayerImplList* list) {
    for (auto* layer_impl : *layer->layer_tree_impl()) {
      if (layer_impl->Is3dSorted() && !layer_impl->bounds().IsEmpty()) {
        list->push_back(layer_impl);
      }
    }
  }

  void AfterTest() override {
    CHECK(!story_.empty()) << "Must SetStory() before TearDown().";
    auto reporter = SetUpBspTreeReporter(story_);
    reporter.AddResult(kMetricCalcDrawPropsTimeUs,
                       timer_.TimePerLap().InMicrosecondsF());
  }

 private:
  cc::FakeContentLayerClient content_layer_client_;
  base::LapTimer timer_;
  std::string story_;
  std::string json_;
  cc::LayerImplList base_list_;
  int num_duplicates_ = 1;
};

TEST_F(BspTreePerfTest, LayerSorterCubes) {
  SetStory("layer_sort_cubes");
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, LayerSorterRubik) {
  SetStory("layer_sort_rubik");
  ReadTestFile("layer_sort_rubik");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeCubes) {
  SetStory("bsp_tree_cubes");
  SetNumberOfDuplicates(1);
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeRubik) {
  SetStory("bsp_tree_rubik");
  SetNumberOfDuplicates(1);
  ReadTestFile("layer_sort_rubik");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeCubes_2) {
  SetStory("bsp_tree_cubes_2");
  SetNumberOfDuplicates(2);
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeCubes_4) {
  SetStory("bsp_tree_cubes_4");
  SetNumberOfDuplicates(4);
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

}  // namespace
}  // namespace viz

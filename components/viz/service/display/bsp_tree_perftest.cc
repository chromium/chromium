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
#include "cc/base/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/transform_node.h"
#include "components/viz/service/display/bsp_tree.h"
#include "components/viz/service/display/draw_polygon.h"
#include "components/viz/test/paths.h"
#include "testing/perf/perf_test.h"

namespace viz {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class BspTreePerfTest : public cc::LayerTreeTest {
 public:
  BspTreePerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void SetupTree() override {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree_host()->SetViewportSizeAndScale(viewport, 1.f, LocalSurfaceId(),
                                               base::TimeTicks());
    scoped_refptr<cc::Layer> root =
        ParseTreeFromJson(json_, &content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree_host()->SetRootLayer(root);
    content_layer_client_.set_bounds(viewport);
  }

  void SetTestName(const std::string& name) { test_name_ = name; }

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
    int max_texture_size = 8096;
    DoCalcDrawPropertiesImpl(max_texture_size, active_tree, host_impl);

    cc::LayerImplList base_list;
    BuildLayerImplList(active_tree->root_layer_for_testing(), &base_list);

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

  void DoCalcDrawPropertiesImpl(int max_texture_size,
                                cc::LayerTreeImpl* active_tree,
                                cc::LayerTreeHostImpl* host_impl) {
    cc::RenderSurfaceList update_list;
    cc::LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        active_tree->root_layer_for_testing(),
        active_tree->GetDeviceViewport().size(), host_impl->DrawTransform(),
        active_tree->device_scale_factor(),
        active_tree->current_page_scale_factor(),
        active_tree->InnerViewportContainerLayer(),
        active_tree->InnerViewportScrollLayer(),
        active_tree->OuterViewportScrollLayer(),
        active_tree->elastic_overscroll()->Current(active_tree->IsActiveTree()),
        active_tree->OverscrollElasticityElementId(), max_texture_size,
        host_impl->settings().layer_transforms_should_scale_layer_contents,
        &update_list, active_tree->property_trees(),
        active_tree->property_trees()->transform_tree.Node(
            active_tree->InnerViewportContainerLayer()
                ->transform_tree_index()));
    cc::LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  void BuildLayerImplList(cc::LayerImpl* layer, cc::LayerImplList* list) {
    for (auto* layer_impl : *layer->layer_tree_impl()) {
      if (layer_impl->Is3dSorted() && !layer_impl->bounds().IsEmpty()) {
        list->push_back(layer_impl);
      }
    }
  }

  void AfterTest() override {
    CHECK(!test_name_.empty()) << "Must SetTestName() before TearDown().";
    perf_test::PrintResult("calc_draw_props_time", "", test_name_,
                           1000 * timer_.MsPerLap(), "us", true);
  }

 private:
  cc::FakeContentLayerClient content_layer_client_;
  cc::LapTimer timer_;
  std::string test_name_;
  std::string json_;
  cc::LayerImplList base_list_;
  int num_duplicates_ = 1;
};

TEST_F(BspTreePerfTest, LayerSorterCubes) {
  SetTestName("layer_sort_cubes");
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, LayerSorterRubik) {
  SetTestName("layer_sort_rubik");
  ReadTestFile("layer_sort_rubik");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeCubes) {
  SetTestName("bsp_tree_cubes");
  SetNumberOfDuplicates(1);
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeRubik) {
  SetTestName("bsp_tree_rubik");
  SetNumberOfDuplicates(1);
  ReadTestFile("layer_sort_rubik");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeCubes_2) {
  SetTestName("bsp_tree_cubes_2");
  SetNumberOfDuplicates(2);
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

TEST_F(BspTreePerfTest, BspTreeCubes_4) {
  SetTestName("bsp_tree_cubes_4");
  SetNumberOfDuplicates(4);
  ReadTestFile("layer_sort_cubes");
  RunTest(cc::CompositorMode::SINGLE_THREADED);
}

}  // namespace
}  // namespace viz

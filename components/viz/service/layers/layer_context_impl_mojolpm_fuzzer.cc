// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "components/viz/service/layers/layer_context_impl_mojolpm_fuzzer.pb.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom-mojolpm.h"
#include "services/viz/public/mojom/compositing/tiling.mojom-mojolpm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace {

struct FuzzerEnvironment {
  FuzzerEnvironment(int* argc, char** argv) {
    base::CommandLine::Init(*argc, argv);
    TestTimeouts::Initialize();
    testing::InitGoogleTest(argc, argv);
    mojo::core::Init();
  }
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

class LayerContextImplTestForFuzzing : public viz::LayerContextImplTest {
 public:
  void TestBody() override {}
  viz::LayerContextImpl* layer_context_impl() {
    return layer_context_impl_.get();
  }
};

void FixUpColorSpace(mojolpm::gfx::mojom::ColorSpace* color_space) {
  if (color_space->instance_case() ==
      mojolpm::gfx::mojom::ColorSpace::INSTANCE_NOT_SET) {
    color_space->mutable_new_();
  }
  if (color_space->instance_case() != mojolpm::gfx::mojom::ColorSpace::kNew) {
    return;
  }
  auto* new_cs = color_space->mutable_new_();

  if (!new_cs->has_m_custom_primary_matrix()) {
    new_cs->mutable_m_custom_primary_matrix();
  }
  while (new_cs->m_custom_primary_matrix().values_size() < 9) {
    new_cs->mutable_m_custom_primary_matrix()->add_values();
  }
  while (new_cs->m_custom_primary_matrix().values_size() > 9) {
    new_cs->mutable_m_custom_primary_matrix()->mutable_values()->RemoveLast();
  }

  if (!new_cs->has_m_transfer_params()) {
    new_cs->mutable_m_transfer_params();
  }
  while (new_cs->m_transfer_params().values_size() < 7) {
    new_cs->mutable_m_transfer_params()->add_values();
  }
  while (new_cs->m_transfer_params().values_size() > 7) {
    new_cs->mutable_m_transfer_params()->mutable_values()->RemoveLast();
  }
}

void FixUpDisplayColorSpaces(mojolpm::gfx::mojom::DisplayColorSpaces* dcs) {
  if (dcs->instance_case() ==
      mojolpm::gfx::mojom::DisplayColorSpaces::INSTANCE_NOT_SET) {
    dcs->mutable_new_();
  }
  if (dcs->instance_case() != mojolpm::gfx::mojom::DisplayColorSpaces::kNew) {
    return;
  }
  auto* new_dcs = dcs->mutable_new_();

  if (!new_dcs->has_m_color_spaces()) {
    new_dcs->mutable_m_color_spaces();
  }
  while (new_dcs->m_color_spaces().values_size() < 6) {
    new_dcs->mutable_m_color_spaces()->add_values();
  }
  while (new_dcs->m_color_spaces().values_size() > 6) {
    new_dcs->mutable_m_color_spaces()->mutable_values()->RemoveLast();
  }
  for (int i = 0; i < new_dcs->m_color_spaces().values_size(); ++i) {
    FixUpColorSpace(
        new_dcs->mutable_m_color_spaces()->mutable_values(i)->mutable_value());
  }

  if (!new_dcs->has_m_formats()) {
    new_dcs->mutable_m_formats();
  }
  while (new_dcs->m_formats().values_size() < 6) {
    new_dcs->mutable_m_formats()->add_values();
  }
  while (new_dcs->m_formats().values_size() > 6) {
    new_dcs->mutable_m_formats()->mutable_values()->RemoveLast();
  }
}

void FixUpTransform(mojolpm::gfx::mojom::Transform* transform) {
  if (transform->instance_case() ==
      mojolpm::gfx::mojom::Transform::INSTANCE_NOT_SET) {
    transform->mutable_new_();
  }
  if (transform->instance_case() != mojolpm::gfx::mojom::Transform::kNew) {
    return;
  }
  auto* new_transform = transform->mutable_new_();
  if (new_transform->has_m_data()) {
    auto* data = new_transform->mutable_m_data();
    if (data->instance_case() == mojolpm::gfx::mojom::TransformData::kNew) {
      auto* data_union = data->mutable_new_();
      if (data_union->union_member_case() ==
          mojolpm::gfx::mojom::TransformData_ProtoUnion::kMMatrix) {
        auto* matrix = data_union->mutable_m_matrix();
        while (matrix->values_size() < 16) {
          matrix->add_values();
        }
        while (matrix->values_size() > 16) {
          matrix->mutable_values()->RemoveLast();
        }
      }
    }
  }
}

void FixUpTransformNode(mojolpm::viz::mojom::TransformNode* node) {
  if (node->instance_case() ==
      mojolpm::viz::mojom::TransformNode::INSTANCE_NOT_SET) {
    node->mutable_new_();
  }
  if (node->instance_case() != mojolpm::viz::mojom::TransformNode::kNew) {
    return;
  }
  auto* new_node = node->mutable_new_();
  if (new_node->has_m_local()) {
    FixUpTransform(new_node->mutable_m_local());
  }
  if (new_node->has_m_to_parent()) {
    FixUpTransform(new_node->mutable_m_to_parent());
  }
}

void FixUpLayerTreeUpdate(mojolpm::viz::mojom::LayerTreeUpdate* update) {
  if (update->instance_case() ==
      mojolpm::viz::mojom::LayerTreeUpdate::INSTANCE_NOT_SET) {
    update->mutable_new_();
  }
  if (update->instance_case() != mojolpm::viz::mojom::LayerTreeUpdate::kNew) {
    return;
  }
  auto* new_update = update->mutable_new_();

  // DisplayColorSpaces is not optional in Mojo, so we must always ensure it
  // exists and has the exactly required 6 elements.
  FixUpDisplayColorSpaces(new_update->mutable_m_display_color_spaces());

  if (new_update->has_m_transform_nodes()) {
    for (int i = 0; i < new_update->m_transform_nodes().values_size(); ++i) {
      if (new_update->mutable_m_transform_nodes()
              ->mutable_values(i)
              ->has_value()) {
        FixUpTransformNode(new_update->mutable_m_transform_nodes()
                               ->mutable_values(i)
                               ->mutable_value());
      }
    }
  }
}

constexpr uint32_t kMaxPropertyTreeNodes = 1000u;
constexpr size_t kMaxLayers = 1000;
constexpr size_t kMaxTilings = 100;
constexpr size_t kMaxTilesPerTiling = 100;
constexpr size_t kMaxUIResourceRequests = 100;
constexpr size_t kMaxSurfaceRanges = 100;
constexpr size_t kMaxLatencyInfo = 100;
constexpr size_t kMaxAnimationTimelines = 100;
constexpr size_t kMaxAnimationsPerTimeline = 100;
constexpr size_t kMaxKeyframeModelsPerAnimation = 10;
constexpr size_t kMaxKeyframesPerAnimationCurve = 10;

void ClampLayerTreeUpdate(viz::mojom::LayerTreeUpdate* update) {
  if (!update) {
    return;
  }

  update->num_transform_nodes =
      std::min(update->num_transform_nodes, kMaxPropertyTreeNodes);
  update->num_clip_nodes =
      std::min(update->num_clip_nodes, kMaxPropertyTreeNodes);
  update->num_effect_nodes =
      std::min(update->num_effect_nodes, kMaxPropertyTreeNodes);
  update->num_scroll_nodes =
      std::min(update->num_scroll_nodes, kMaxPropertyTreeNodes);

  if (update->transform_nodes.size() > kMaxPropertyTreeNodes) {
    update->transform_nodes.resize(kMaxPropertyTreeNodes);
  }
  if (update->clip_nodes.size() > kMaxPropertyTreeNodes) {
    update->clip_nodes.resize(kMaxPropertyTreeNodes);
  }
  if (update->effect_nodes.size() > kMaxPropertyTreeNodes) {
    update->effect_nodes.resize(kMaxPropertyTreeNodes);
  }
  if (update->scroll_nodes.size() > kMaxPropertyTreeNodes) {
    update->scroll_nodes.resize(kMaxPropertyTreeNodes);
  }

  if (update->layers.size() > kMaxLayers) {
    update->layers.resize(kMaxLayers);
  }
  if (update->layer_order && update->layer_order->size() > kMaxLayers) {
    update->layer_order->resize(kMaxLayers);
  }

  if (update->tilings.size() > kMaxTilings) {
    update->tilings.resize(kMaxTilings);
  }
  for (auto& tiling : update->tilings) {
    if (tiling->tiles.size() > kMaxTilesPerTiling) {
      tiling->tiles.resize(kMaxTilesPerTiling);
    }
  }

  if (update->ui_resource_requests.size() > kMaxUIResourceRequests) {
    update->ui_resource_requests.resize(kMaxUIResourceRequests);
  }
  if (update->surface_ranges &&
      update->surface_ranges->size() > kMaxSurfaceRanges) {
    update->surface_ranges->resize(kMaxSurfaceRanges);
  }
  if (update->latency_info.size() > kMaxLatencyInfo) {
    update->latency_info.resize(kMaxLatencyInfo);
  }

  // Note: view_transition_requests might not exist if they weren't added in
  // mojom yet, let's skip unless we are sure. We saw
  // DeserializeViewTransitionRequests, so it should exist. Actually, I don't
  // know the exact names. Let's just limit what we are sure about.

  if (update->removed_animation_timelines &&
      update->removed_animation_timelines->size() > kMaxAnimationTimelines) {
    update->removed_animation_timelines->resize(kMaxAnimationTimelines);
  }
  if (update->animation_timelines) {
    if (update->animation_timelines->size() > kMaxAnimationTimelines) {
      update->animation_timelines->resize(kMaxAnimationTimelines);
    }
    for (auto& timeline : *update->animation_timelines) {
      if (timeline->removed_animations.size() > kMaxAnimationsPerTimeline) {
        timeline->removed_animations.resize(kMaxAnimationsPerTimeline);
      }
      if (timeline->new_animations.size() > kMaxAnimationsPerTimeline) {
        timeline->new_animations.resize(kMaxAnimationsPerTimeline);
      }
      for (auto& animation : timeline->new_animations) {
        if (animation->keyframe_models.size() >
            kMaxKeyframeModelsPerAnimation) {
          animation->keyframe_models.resize(kMaxKeyframeModelsPerAnimation);
        }
        for (auto& model : animation->keyframe_models) {
          if (model->keyframes.size() > kMaxKeyframesPerAnimationCurve) {
            model->keyframes.resize(kMaxKeyframesPerAnimationCurve);
          }
        }
      }
    }
  }
}

class LayerContextTestcase
    : public mojolpm::Testcase<viz::fuzzing::layer_context::proto::Testcase,
                               viz::fuzzing::layer_context::proto::Action> {
 public:
  using ProtoTestcase = viz::fuzzing::layer_context::proto::Testcase;
  using ProtoAction = viz::fuzzing::layer_context::proto::Action;

  explicit LayerContextTestcase(const ProtoTestcase& testcase)
      : mojolpm::Testcase<ProtoTestcase, ProtoAction>(testcase) {}

  ~LayerContextTestcase() = default;

  void SetUp(base::OnceClosure done_closure) override {
    impl_test_ = std::make_unique<LayerContextImplTestForFuzzing>();
    impl_test_->SetUp();

    // Give it a valid initial state just like the unit tests do.
    auto default_update = impl_test_->CreateDefaultUpdate();
    (void)impl_test_->layer_context_impl()->DoUpdateDisplayTree(
        std::move(default_update));

    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void TearDown(base::OnceClosure done_closure) override {
    impl_test_.reset();
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void RunAction(const ProtoAction& action,
                 base::OnceClosure run_closure) override {
    switch (action.action_case()) {
      case ProtoAction::kRunThread:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
        return;

      case ProtoAction::kUpdateDisplayTree: {
        viz::mojom::LayerTreeUpdatePtr update;
        auto proto_update = action.update_display_tree();
        FixUpLayerTreeUpdate(&proto_update);
        if (mojolpm::FromProto(proto_update, update) && update) {
          ClampLayerTreeUpdate(update.get());
          // Call the implementation directly, bypassing the Mojo pipe to avoid
          // ReportBadMessage disconnecting the endpoint on invalid fuzz data.
          (void)impl_test_->layer_context_impl()->DoUpdateDisplayTree(
              std::move(update));
        }
        break;
      }

      case ProtoAction::kUpdateDisplayTiling: {
        viz::mojom::TilingPtr tiling;
        if (mojolpm::FromProto(action.update_display_tiling(), tiling) &&
            tiling) {
          (void)impl_test_->layer_context_impl()->DoUpdateDisplayTiling(
              std::move(tiling));
        }
        break;
      }

      case ProtoAction::ACTION_NOT_SET:
        break;
    }

    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
  }

 private:
  std::unique_ptr<LayerContextImplTestForFuzzing> impl_test_;
};

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(
    const viz::fuzzing::layer_context::proto::Testcase& testcase) {
  if (!testcase.actions_size() && !testcase.sequences_size()) {
    return;
  }

  int argc = 1;
  const char* argv_array[] = {"layer_context_impl_mojolpm_fuzzer"};
  char** argv = const_cast<char**>(argv_array);
  static FuzzerEnvironment env(&argc, argv);

  LayerContextTestcase testcase_runner(testcase);

  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<LayerContextTestcase>,
                     base::Unretained(&testcase_runner), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class LayerContextImplViewTransitionRequestTest : public LayerContextImplTest {
};

TEST_F(LayerContextImplViewTransitionRequestTest, DeserializeSaveDirective) {
  constexpr uint32_t kSequenceId = 123;
  constexpr bool kMaybeCrossFrameSink = false;

  auto update = CreateDefaultUpdate();
  auto request_mojom = mojom::ViewTransitionRequest::New();
  request_mojom->sequence_id = kSequenceId;
  request_mojom->type = mojom::CompositorFrameTransitionDirectiveType::kSave;
  request_mojom->transition_token = blink::ViewTransitionToken();
  request_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink;

  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();
  update->view_transition_requests->push_back(std::move(request_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& requests_impl = active_tree->view_transition_requests();
  ASSERT_EQ(requests_impl.size(), 1u);
  EXPECT_EQ(requests_impl[0]->sequence_id(), kSequenceId);
  EXPECT_EQ(requests_impl[0]->type(), cc::ViewTransitionRequest::Type::kSave);
  EXPECT_TRUE(requests_impl[0]->capture_resource_ids().empty());
  EXPECT_EQ(requests_impl[0]->maybe_cross_frame_sink(), kMaybeCrossFrameSink);
}

TEST_F(LayerContextImplViewTransitionRequestTest,
       DeserializeSaveDirectiveWithCaptureIds) {
  constexpr uint32_t kSequenceId = 456;
  constexpr bool kMaybeCrossFrameSink = true;

  auto update = CreateDefaultUpdate();
  auto request_mojom = mojom::ViewTransitionRequest::New();
  request_mojom->sequence_id = kSequenceId;
  request_mojom->type = mojom::CompositorFrameTransitionDirectiveType::kSave;
  request_mojom->transition_token = blink::ViewTransitionToken();
  request_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink;

  ViewTransitionElementResourceId resource_id1(blink::ViewTransitionToken(), 1,
                                               true);
  ViewTransitionElementResourceId resource_id2(blink::ViewTransitionToken(), 2,
                                               false);
  request_mojom->capture_resource_ids.push_back(resource_id1);
  request_mojom->capture_resource_ids.push_back(resource_id2);

  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();
  update->view_transition_requests->push_back(std::move(request_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& requests_impl = active_tree->view_transition_requests();
  ASSERT_EQ(requests_impl.size(), 1u);
  EXPECT_EQ(requests_impl[0]->sequence_id(), kSequenceId);
  EXPECT_EQ(requests_impl[0]->type(), cc::ViewTransitionRequest::Type::kSave);
  ASSERT_EQ(requests_impl[0]->capture_resource_ids().size(), 2u);
  EXPECT_EQ(requests_impl[0]->capture_resource_ids()[0], resource_id1);
  EXPECT_EQ(requests_impl[0]->capture_resource_ids()[1], resource_id2);
  EXPECT_EQ(requests_impl[0]->maybe_cross_frame_sink(), kMaybeCrossFrameSink);
}

TEST_F(LayerContextImplViewTransitionRequestTest,
       DeserializeSaveDirectiveWithEmptyCaptureIds) {
  constexpr uint32_t kSequenceId = 789;
  constexpr bool kMaybeCrossFrameSink = false;

  auto update = CreateDefaultUpdate();
  auto request_mojom = mojom::ViewTransitionRequest::New();
  request_mojom->sequence_id = kSequenceId;
  request_mojom->type = mojom::CompositorFrameTransitionDirectiveType::kSave;
  request_mojom->transition_token = blink::ViewTransitionToken();
  request_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink;
  // capture_resource_ids is explicitly empty.
  request_mojom->capture_resource_ids =
      std::vector<ViewTransitionElementResourceId>();

  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();
  update->view_transition_requests->push_back(std::move(request_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& requests_impl = active_tree->view_transition_requests();
  ASSERT_EQ(requests_impl.size(), 1u);
  EXPECT_EQ(requests_impl[0]->sequence_id(), kSequenceId);
  EXPECT_EQ(requests_impl[0]->type(), cc::ViewTransitionRequest::Type::kSave);
  EXPECT_TRUE(requests_impl[0]->capture_resource_ids().empty());
  EXPECT_EQ(requests_impl[0]->maybe_cross_frame_sink(), kMaybeCrossFrameSink);
}

TEST_F(LayerContextImplViewTransitionRequestTest,
       DeserializeAnimateRendererDirective) {
  constexpr uint32_t kSequenceId = 101;
  constexpr bool kMaybeCrossFrameSink = false;

  auto update = CreateDefaultUpdate();
  auto request_mojom = mojom::ViewTransitionRequest::New();
  request_mojom->sequence_id = kSequenceId;
  request_mojom->type =
      mojom::CompositorFrameTransitionDirectiveType::kAnimateRenderer;
  request_mojom->transition_token = blink::ViewTransitionToken();
  request_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink;

  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();
  update->view_transition_requests->push_back(std::move(request_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& requests_impl = active_tree->view_transition_requests();
  ASSERT_EQ(requests_impl.size(), 1u);
  EXPECT_EQ(requests_impl[0]->sequence_id(), kSequenceId);
  EXPECT_EQ(requests_impl[0]->type(),
            cc::ViewTransitionRequest::Type::kAnimateRenderer);
  EXPECT_EQ(requests_impl[0]->maybe_cross_frame_sink(), kMaybeCrossFrameSink);
}

TEST_F(LayerContextImplViewTransitionRequestTest, DeserializeReleaseDirective) {
  constexpr uint32_t kSequenceId = 202;
  constexpr bool kMaybeCrossFrameSink = false;

  auto update = CreateDefaultUpdate();
  auto request_mojom = mojom::ViewTransitionRequest::New();
  request_mojom->sequence_id = kSequenceId;
  request_mojom->type = mojom::CompositorFrameTransitionDirectiveType::kRelease;
  request_mojom->transition_token = blink::ViewTransitionToken();
  request_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink;

  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();
  update->view_transition_requests->push_back(std::move(request_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& requests_impl = active_tree->view_transition_requests();
  ASSERT_EQ(requests_impl.size(), 1u);
  EXPECT_EQ(requests_impl[0]->sequence_id(), kSequenceId);
  EXPECT_EQ(requests_impl[0]->type(),
            cc::ViewTransitionRequest::Type::kRelease);
  EXPECT_EQ(requests_impl[0]->maybe_cross_frame_sink(), kMaybeCrossFrameSink);
}

TEST_F(LayerContextImplViewTransitionRequestTest,
       DeserializeMultipleDirectives) {
  constexpr uint32_t kSequenceId1 = 301;
  constexpr bool kMaybeCrossFrameSink1 = false;
  constexpr uint32_t kSequenceId2 = 302;
  constexpr bool kMaybeCrossFrameSink2 = true;

  auto update = CreateDefaultUpdate();
  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();

  auto request1_mojom = mojom::ViewTransitionRequest::New();
  request1_mojom->sequence_id = kSequenceId1;
  request1_mojom->type = mojom::CompositorFrameTransitionDirectiveType::kSave;
  request1_mojom->transition_token = blink::ViewTransitionToken();
  request1_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink1;
  update->view_transition_requests->push_back(std::move(request1_mojom));

  auto request2_mojom = mojom::ViewTransitionRequest::New();
  request2_mojom->sequence_id = kSequenceId2;
  request2_mojom->type =
      mojom::CompositorFrameTransitionDirectiveType::kAnimateRenderer;
  request2_mojom->transition_token = blink::ViewTransitionToken();
  request2_mojom->maybe_cross_frame_sink = kMaybeCrossFrameSink2;
  update->view_transition_requests->push_back(std::move(request2_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& requests_impl = active_tree->view_transition_requests();
  ASSERT_EQ(requests_impl.size(), 2u);

  EXPECT_EQ(requests_impl[0]->sequence_id(), kSequenceId1);
  EXPECT_EQ(requests_impl[0]->type(), cc::ViewTransitionRequest::Type::kSave);
  EXPECT_EQ(requests_impl[0]->maybe_cross_frame_sink(), kMaybeCrossFrameSink1);

  EXPECT_EQ(requests_impl[1]->sequence_id(), kSequenceId2);
  EXPECT_EQ(requests_impl[1]->type(),
            cc::ViewTransitionRequest::Type::kAnimateRenderer);
  EXPECT_EQ(requests_impl[1]->maybe_cross_frame_sink(), kMaybeCrossFrameSink2);
}

TEST_F(LayerContextImplViewTransitionRequestTest,
       DeserializeNoViewTransitionRequests) {
  auto update = CreateDefaultUpdate();
  // view_transition_requests is null by default in CreateDefaultUpdate.
  // Explicitly set to empty vector for clarity.
  update->view_transition_requests =
      std::vector<mojom::ViewTransitionRequestPtr>();

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  EXPECT_TRUE(active_tree->view_transition_requests().empty());
}

}  // namespace
}  // namespace viz

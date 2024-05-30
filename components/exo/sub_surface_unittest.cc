// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/sub_surface.h"

#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace exo {
namespace {

using SubSurfaceTest = test::ExoTestBase;

TEST_F(SubSurfaceTest, SetPosition) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto surface = std::make_unique<Surface>();
  auto sub_surface = std::make_unique<SubSurface>(surface.get(), parent.get());

  // Initial position is at the origin.
  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->bounds().origin().ToString());

  // Set position to 10, 10.
  gfx::PointF position(10, 10);
  sub_surface->SetPosition(position);

  // A call to Commit() is required for position to take effect.
  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->bounds().origin().ToString());

  // Check that position is updated when Commit() is called.
  parent->Commit();
  EXPECT_EQ(gfx::ToRoundedPoint(position).ToString(),
            surface->window()->bounds().origin().ToString());

  // Create and commit a new sub-surface using the same surface.
  sub_surface.reset();
  sub_surface = std::make_unique<SubSurface>(surface.get(), parent.get());
  parent->Commit();

  // Initial position should be reset to origin.
  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->bounds().origin().ToString());
}

TEST_F(SubSurfaceTest, PlaceAbove) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto surface1 = std::make_unique<Surface>();
  auto surface2 = std::make_unique<Surface>();
  auto non_sibling_surface = std::make_unique<Surface>();
  auto sub_surface1 =
      std::make_unique<SubSurface>(surface1.get(), parent.get());
  auto sub_surface2 =
      std::make_unique<SubSurface>(surface2.get(), parent.get());

  ASSERT_EQ(2u, parent->window()->children().size());
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().back().first);

  sub_surface2->PlaceAbove(parent.get());
  sub_surface1->PlaceAbove(non_sibling_surface.get());  // Invalid
  sub_surface1->PlaceAbove(surface1.get());             // Invalid
  sub_surface1->PlaceAbove(surface2.get());

  // Nothing should have changed as Commit() is required for new stacking
  // order to take effect.
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().back().first);

  parent->Commit();

  // surface1 should now be stacked above surface2.
  EXPECT_EQ(surface2->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface1->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().back().first);

  sub_surface1->PlaceAbove(parent.get());
  parent->Commit();

  // surface2 should now be stacked above surface1.
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().back().first);
}

TEST_F(SubSurfaceTest, PlaceBelow) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto surface1 = std::make_unique<Surface>();
  auto surface2 = std::make_unique<Surface>();
  auto non_sibling_surface = std::make_unique<Surface>();
  auto sub_surface1 =
      std::make_unique<SubSurface>(surface1.get(), parent.get());
  auto sub_surface2 =
      std::make_unique<SubSurface>(surface2.get(), parent.get());

  ASSERT_EQ(2u, parent->window()->children().size());
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().back().first);

  sub_surface2->PlaceBelow(parent.get());               // Invalid
  sub_surface2->PlaceBelow(non_sibling_surface.get());  // Invalid
  sub_surface1->PlaceBelow(surface2.get());
  sub_surface2->PlaceBelow(surface1.get());

  // Nothing should have changed as Commit() is required for new stacking
  // order to take effect.
  EXPECT_EQ(surface1->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface2->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().back().first);

  parent->Commit();

  // surface1 should now be stacked above surface2.
  EXPECT_EQ(surface2->window(), parent->window()->children()[0]);
  EXPECT_EQ(surface1->window(), parent->window()->children()[1]);
  EXPECT_EQ(surface2.get(), parent->sub_surfaces().front().first);
  EXPECT_EQ(surface1.get(), parent->sub_surfaces().back().first);
}

TEST_F(SubSurfaceTest, ParentDamageOnReorder) {
  gfx::Size buffer_size(800, 600);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("SubSurfaceTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  auto parent = std::make_unique<Surface>();
  parent->Attach(buffer.get());
  // Set the overlay priority hint to low to prevent a texture draw quad from
  // being used.
  parent->SetOverlayPriorityHint(OverlayPriority::LOW);
  auto surface1 = std::make_unique<Surface>();
  auto surface2 = std::make_unique<Surface>();
  auto non_sibling_surface = std::make_unique<Surface>();
  auto sub_surface1 =
      std::make_unique<SubSurface>(surface1.get(), parent.get());
  auto sub_surface2 =
      std::make_unique<SubSurface>(surface2.get(), parent.get());

  sub_surface2->PlaceBelow(surface1.get());
  parent->Commit();

  viz::CompositorFrame frame1;
  frame1.render_pass_list.push_back(viz::CompositorRenderPass::Create());
  parent->AppendSurfaceHierarchyContentsToFrame(
      gfx::PointF{}, gfx::PointF{},
      /*needs_full_damage=*/false, frame_sink_holder->resource_manager(),
      /*device_scale_factor=*/std::nullopt, &frame1);

  // Parent surface damage is extended when sub_surface stacking order changes.
  EXPECT_FALSE(frame1.render_pass_list.back()->damage_rect.IsEmpty());

  sub_surface1->PlaceAbove(surface2.get());  // no-op
  sub_surface2->PlaceBelow(surface1.get());  // no-op
  parent->Commit();

  viz::CompositorFrame frame2;
  frame2.render_pass_list.push_back(viz::CompositorRenderPass::Create());
  parent->AppendSurfaceHierarchyContentsToFrame(
      gfx::PointF{}, gfx::PointF{},
      /*needs_full_damage=*/false, frame_sink_holder->resource_manager(),
      /*device_scale_factor=*/std::nullopt, &frame2);

  // Parent surface damage is unaffected.
  EXPECT_TRUE(frame2.render_pass_list.back()->damage_rect.IsEmpty());
}

TEST_F(SubSurfaceTest, SetCommitBehavior) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  auto child = std::make_unique<Surface>();
  auto grandchild = std::make_unique<Surface>();
  auto child_sub_surface =
      std::make_unique<SubSurface>(child.get(), parent.get());
  auto grandchild_sub_surface =
      std::make_unique<SubSurface>(grandchild.get(), child.get());

  // Initial position is at the origin.
  EXPECT_EQ(gfx::Point().ToString(),
            grandchild->window()->bounds().origin().ToString());

  // Set position to 10, 10.
  gfx::PointF position1(10, 10);
  grandchild_sub_surface->SetPosition(position1);
  child->Commit();

  // Initial commit behavior is synchronous and the effect of the child
  // Commit() call will not take effect until Commit() is called on the
  // parent.
  EXPECT_EQ(gfx::Point().ToString(),
            grandchild->window()->bounds().origin().ToString());

  parent->Commit();

  // Position should have been updated when Commit() has been called on both
  // child and parent.
  EXPECT_EQ(gfx::ToRoundedPoint(position1).ToString(),
            grandchild->window()->bounds().origin().ToString());

  bool synchronized = false;
  child_sub_surface->SetCommitBehavior(synchronized);

  // Set position to 20, 20.
  gfx::PointF position2(20, 20);
  grandchild_sub_surface->SetPosition(position2);
  child->Commit();

  // A Commit() call on child should be sufficient for the position of
  // grandchild to take effect when synchronous is disabled.
  EXPECT_EQ(gfx::ToRoundedPoint(position2).ToString(),
            grandchild->window()->bounds().origin().ToString());
}

TEST_F(SubSurfaceTest, SetOnParent) {
  gfx::Size buffer_size(32, 32);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());
  parent->Attach(buffer.get());
  parent->Commit();

  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);
  ASSERT_TRUE(parent->window()->GetProperty(aura::client::kSkipImeProcessing));

  // SkipImeProcessing property is propagated to SubSurface.
  auto surface = std::make_unique<Surface>();
  auto sub_surface = std::make_unique<SubSurface>(surface.get(), parent.get());
  surface->SetParent(parent.get(), gfx::Point(10, 10));
  EXPECT_TRUE(surface->window()->GetProperty(aura::client::kSkipImeProcessing));
}

TEST_F(SubSurfaceTest, AugmentedSurfaceDoesNotExpandHierarchy) {
  auto parent = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(parent.get());

  auto sub_layer = std::make_unique<Surface>();
  sub_layer->set_is_augmented(true);
  auto sub_surface = std::make_unique<Surface>();
  auto sub_layer_role =
      std::make_unique<SubSurface>(sub_layer.get(), parent.get());
  auto sub_surface_role =
      std::make_unique<SubSurface>(sub_surface.get(), parent.get());

  auto parent_buffer = exo_test_helper()->CreateBuffer(gfx::Size(800, 600));
  auto sub_surface_buffer = exo_test_helper()->CreateBuffer(gfx::Size(10, 10));
  auto sub_layer_buffer = exo_test_helper()->CreateBuffer(gfx::Size(10, 10));

  // Set position to be outside parent surface.
  sub_layer_role->SetPosition(gfx::PointF(-10, 0));
  sub_surface_role->SetPosition(gfx::PointF(0, -10));

  parent->Attach(parent_buffer.get());
  sub_layer->Attach(sub_layer_buffer.get());
  sub_surface->Attach(sub_surface_buffer.get());

  sub_layer->Commit();
  sub_surface->Commit();
  parent->Commit();

  // Only sub_surface affects the surface hierarchy bounds
  EXPECT_EQ(parent->surface_hierarchy_content_bounds(),
            gfx::Rect(0, -10, 800, 610));
  // sub_layer is not in parent aura window's children list.
  EXPECT_EQ(1u, parent->window()->children().size());
  EXPECT_EQ(sub_surface->window(), parent->window()->children()[0]);
}

}  // namespace
}  // namespace exo

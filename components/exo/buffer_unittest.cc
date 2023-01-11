// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <GLES2/gl2extchromium.h>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/exo/surface_tree_host.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/in_process_context_provider.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

using BufferTest = test::ExoTestBase;

void Release(int* release_call_count) {
  (*release_call_count)++;
}

void ExplicitRelease(int* release_call_count,
                     gfx::GpuFenceHandle release_fence) {
  (*release_call_count)++;
}

void VerifySyncTokensInCompositorFrame(viz::CompositorFrame* frame) {
  std::vector<GLbyte*> sync_tokens;
  for (auto& resource : frame->resource_list)
    sync_tokens.push_back(resource.mailbox_holder.sync_token.GetData());
  gpu::raster::RasterInterface* ri =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider()
          ->RasterInterface();
  ri->VerifySyncTokensCHROMIUM(sync_tokens.data(), sync_tokens.size());
}

TEST_F(BufferTest, ReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // This is needed to ensure that base::RunLoop().RunUntilIdle() call below
  // is always sufficient for buffer to be released.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::BindRepeating(&Release, base::Unretained(&release_call_count)));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource, nullptr,
      base::BindOnce(&ExplicitRelease,
                     base::Unretained(&release_resource_count)));
  ASSERT_TRUE(rv);

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(resource.id, resource.mailbox_holder.sync_token,
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(release_call_count, 0);

  // The resource should have been released even if the whole buffer hasn't.
  ASSERT_EQ(release_resource_count, 1);

  buffer->OnDetach();

  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, SolidColorReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<SolidColorBuffer>(SkColors::kRed, buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // This is needed to ensure that base::RunLoop().RunUntilIdle() call below
  // is always sufficient for buffer to be released.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::BindRepeating(&Release, base::Unretained(&release_call_count)));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource, nullptr,
      base::BindOnce(&ExplicitRelease,
                     base::Unretained(&release_resource_count)));
  // Solid color buffer is immediately released after commit.
  EXPECT_EQ(release_resource_count, 1);
  EXPECT_FALSE(rv);

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(resource.id, resource.mailbox_holder.sync_token,
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(release_call_count, 0);

  buffer->OnDetach();

  // Release() should have been called exactly once.
  EXPECT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, IsLost) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  buffer->OnAttach();
  // Acquire a texture transferable resource for the contents of the buffer.
  viz::TransferableResource resource;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource, nullptr,
      base::DoNothing());
  ASSERT_TRUE(rv);

  scoped_refptr<viz::RasterContextProvider> context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider();
  if (context_provider) {
    gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
    ri->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                            GL_INNOCENT_CONTEXT_RESET_ARB);
  }

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(resource.id, gpu::SyncToken(),
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/true);
  frame_sink_holder->ReclaimResources(std::move(resources));
  base::RunLoop().RunUntilIdle();

  // Producing a new texture transferable resource for the contents of the
  // buffer.
  viz::TransferableResource new_resource;
  rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &new_resource,
      nullptr, base::DoNothing());
  ASSERT_TRUE(rv);
  buffer->OnDetach();

  std::vector<viz::ReturnedResource> resources2;
  resources2.emplace_back(new_resource.id, gpu::SyncToken(),
                          /*release_fence=*/gfx::GpuFenceHandle(),
                          /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources2));
  base::RunLoop().RunUntilIdle();
}

// Buffer::Texture::OnLostResources is called when the gpu crashes. This test
// verifies that the Texture is collected properly in such event.
TEST_F(BufferTest, OnLostResources) {
  // Create a Buffer and use it to produce a Texture.
  constexpr gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  buffer->OnAttach();
  // Acquire a texture transferable resource for the contents of the buffer.
  viz::TransferableResource resource;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource, nullptr,
      base::DoNothing());
  ASSERT_TRUE(rv);

  viz::RasterContextProvider* context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider()
          .get();
  static_cast<ui::InProcessContextProvider*>(context_provider)
      ->SendOnContextLost();
}

TEST_F(BufferTest, SurfaceTreeHostDestruction) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // This is needed to ensure that base::RunLoop().RunUntilIdle() call below
  // is always sufficient for buffer to be released.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::BindRepeating(&Release, base::Unretained(&release_call_count)));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource, nullptr,
      base::BindOnce(&ExplicitRelease,
                     base::Unretained(&release_resource_count)));
  ASSERT_TRUE(rv);

  // Submit frame with resource.
  {
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id.source_id =
        viz::BeginFrameArgs::kManualSourceId;
    frame.metadata.begin_frame_ack.frame_id.sequence_number =
        viz::BeginFrameArgs::kStartingFrameNumber;
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.frame_token = 1;
    frame.metadata.device_scale_factor = 1;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(buffer_size),
                 gfx::Rect(buffer_size), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    frame.resource_list.push_back(resource);
    VerifySyncTokensInCompositorFrame(&frame);
    frame_sink_holder->SubmitCompositorFrame(std::move(frame));
  }

  buffer->OnDetach();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(release_call_count, 0);
  ASSERT_EQ(release_resource_count, 0);

  surface_tree_host.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(release_call_count, 1);
  ASSERT_EQ(release_resource_count, 1);
}

TEST_F(BufferTest, SurfaceTreeHostLastFrame) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // This is needed to ensure that base::RunLoop().RunUntilIdle() call below
  // is always sufficient for buffer to be released.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::BindRepeating(&Release, base::Unretained(&release_call_count)));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource, nullptr,
      base::BindOnce(&ExplicitRelease,
                     base::Unretained(&release_resource_count)));
  ASSERT_TRUE(rv);

  // Submit frame with resource.
  {
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id =
        viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                          viz::BeginFrameArgs::kStartingFrameNumber);
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.frame_token = 1;
    frame.metadata.device_scale_factor = 1;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(buffer_size),
                 gfx::Rect(buffer_size), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    frame.resource_list.push_back(resource);
    VerifySyncTokensInCompositorFrame(&frame);
    frame_sink_holder->SubmitCompositorFrame(std::move(frame));

    // Try to release buffer in last frame. This can happen during a resize
    // when frame sink id changes.
    std::vector<viz::ReturnedResource> resources;
    resources.emplace_back(resource.id, resource.mailbox_holder.sync_token,
                           /*release_fence=*/gfx::GpuFenceHandle(),
                           /*count=*/0, /*lost=*/false);
    frame_sink_holder->ReclaimResources(std::move(resources));
  }

  base::RunLoop().RunUntilIdle();
  buffer->OnDetach();

  // Release() should not have been called as resource is used by last frame.
  ASSERT_EQ(release_call_count, 0);
  ASSERT_EQ(release_resource_count, 0);

  // Submit frame without resource. This should cause buffer to be released.
  {
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id =
        viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                          viz::BeginFrameArgs::kStartingFrameNumber);
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.frame_token = 1;
    frame.metadata.device_scale_factor = 1;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(buffer_size),
                 gfx::Rect(buffer_size), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    frame_sink_holder->SubmitCompositorFrame(std::move(frame));
  }

  base::RunLoop().RunUntilIdle();
  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
  ASSERT_EQ(release_resource_count, 1);
}

}  // namespace
}  // namespace exo

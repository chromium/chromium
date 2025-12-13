// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <GLES2/gl2extchromium.h>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface_tree_host.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/surface_tree_host_test_util.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace exo {
namespace {

class BufferTest : public test::ExoTestBase {
 public:
  BufferTest()
      : test::ExoTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }
};

void VerifySyncTokensInCompositorFrame(viz::CompositorFrame* frame) {
  std::vector<GLbyte*> sync_tokens;
  for (auto& resource : frame->resource_list)
    sync_tokens.push_back(resource.mutable_sync_token().GetData());
  gpu::raster::RasterInterface* ri =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider()
          ->RasterInterface();
  ri->VerifySyncTokensCHROMIUM(sync_tokens.data(), sync_tokens.size());
}

viz::CompositorFrame CreateCompositorFrame(
    SurfaceTreeHost* surface_tree_host,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    std::vector<viz::TransferableResource> resources) {
  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack.frame_id =
      viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                        viz::BeginFrameArgs::kStartingFrameNumber);
  frame.metadata.begin_frame_ack.has_damage = true;
  frame.metadata.frame_token = surface_tree_host->GenerateNextFrameToken();
  frame.metadata.device_scale_factor = 1;
  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, damage_rect,
               gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  frame.resource_list = std::move(resources);
  if (!frame.resource_list.empty()) {
    VerifySyncTokensInCompositorFrame(&frame);
  }
  return frame;
}

TEST_F(BufferTest, ReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  base::RunLoop run_loop_1;
  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_call_count, run_loop_1.QuitClosure()));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(resource->id, resource->sync_token(),
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources));

  buffer->OnDetach();

  run_loop_1.Run();
  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, SolidColorReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<SolidColorBuffer>(SkColors::kRed, buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  base::RunLoop run_loop;
  buffer->set_release_callback(
      test::CreateReleaseBufferClosure(&release_call_count, /*closure=*/{}));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  // Solid color buffer is immediately released after commit.
  EXPECT_FALSE(resource);

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(viz::kInvalidResourceId, gpu::SyncToken(),
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources));

  // We expect that Release() is not called, no matter whether we have a wait
  // here or how long the wait is. An arbitrary time period is added here so
  // that if the event mistakenly happens, it is more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(release_call_count, 0);

  buffer->OnDetach();

  // Release() should never be called.
  EXPECT_EQ(release_call_count, 0);
}

TEST_F(BufferTest, IsLost) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  buffer->OnAttach();
  {
    // Acquire a texture transferable resource for the contents of the buffer.
    std::optional<viz::TransferableResource> resource =
        buffer->ProduceTransferableResource(
            frame_sink_holder->resource_manager(), nullptr, false,
            gfx::ColorSpace::CreateSRGB(), nullptr);
    ASSERT_TRUE(resource);

    scoped_refptr<viz::RasterContextProvider> context_provider =
        aura::Env::GetInstance()
            ->context_factory()
            ->SharedMainThreadRasterContextProvider();
    if (context_provider) {
      static_cast<viz::TestInProcessContextProvider*>(context_provider.get())
          ->SendOnContextLost();
    }

    // Release buffer.
    std::vector<viz::ReturnedResource> resources;
    resources.emplace_back(resource->id, gpu::SyncToken(),
                           /*release_fence=*/gfx::GpuFenceHandle(),
                           /*count=*/0, /*lost=*/true);
    frame_sink_holder->ReclaimResources(std::move(resources));
  }

  {
    // Producing a new texture transferable resource for the contents of the
    // buffer.
    std::optional<viz::TransferableResource> new_resource =
        buffer->ProduceTransferableResource(
            frame_sink_holder->resource_manager(), nullptr, false,
            gfx::ColorSpace::CreateSRGB(), nullptr);
    ASSERT_TRUE(new_resource);
    buffer->OnDetach();

    std::vector<viz::ReturnedResource> resources2;
    resources2.emplace_back(new_resource->id, gpu::SyncToken(),
                            /*release_fence=*/gfx::GpuFenceHandle(),
                            /*count=*/0, /*lost=*/false);
    frame_sink_holder->ReclaimResources(std::move(resources2));
  }
}

// Buffer::Texture::OnLostResources is called when the gpu crashes. This test
// verifies that the Texture is collected properly in such event.
TEST_F(BufferTest, OnLostResources) {
  // Create a Buffer and use it to produce a Texture.
  constexpr gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  buffer->OnAttach();
  // Acquire a texture transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  viz::RasterContextProvider* context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider()
          .get();
  static_cast<viz::TestInProcessContextProvider*>(context_provider)
      ->SendOnContextLost();
}

TEST_F(BufferTest, SurfaceTreeHostDestruction) {
  gfx::Size buffer_size(256, 256);

  // We need to setup shell surface and commit the surface, which properly
  // registers frame sink hierarchy and attaches begin frame source. Otherwise
  // OnBeginFrame requests won't be sent.
  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();
  test::WaitForLastFrameAck(shell_surface.get());

  LayerTreeFrameSinkHolder* frame_sink_holder =
      shell_surface->layer_tree_frame_sink_holder();

  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop;
  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_call_count, run_loop.QuitClosure()));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  // Submit frame with resource.
  shell_surface->SubmitCompositorFrameForTesting(
      CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                            gfx::Rect(buffer_size), {resource.value()}));
  test::WaitForLastFrameAck(shell_surface.get());

  buffer->OnDetach();

  // We expect that the buffer and resource should not be released yet, no
  // matter whether we have a wait here or how long the wait is. An arbitrary
  // time period is added here so that if the event mistakenly happens, it is
  // more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));
  ASSERT_EQ(release_call_count, 0);

  shell_surface.reset();
  run_loop.Run();
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, SurfaceTreeHostLastFrame) {
  gfx::Size buffer_size(256, 256);

  // We need to setup shell surface and commit the surface, which properly
  // registers frame sink hierarchy and attaches begin frame source. Otherwise
  // OnBeginFrame requests won't be sent.
  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();
  test::WaitForLastFrameAck(shell_surface.get());

  LayerTreeFrameSinkHolder* frame_sink_holder =
      shell_surface->layer_tree_frame_sink_holder();

  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop;

  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_call_count, run_loop.QuitClosure()));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  // Submit frame with resource.
  {
    shell_surface->SubmitCompositorFrameForTesting(
        CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                              gfx::Rect(buffer_size), {resource.value()}));
    test::WaitForLastFrameAck(shell_surface.get());

    // Try to release buffer in last frame. This can happen during a resize
    // when frame sink id changes.
    std::vector<viz::ReturnedResource> resources;
    resources.emplace_back(resource->id, resource->sync_token(),
                           /*release_fence=*/gfx::GpuFenceHandle(),
                           /*count=*/0, /*lost=*/false);
    frame_sink_holder->ReclaimResources(std::move(resources));
  }

  buffer->OnDetach();

  // We expect that the buffer and resource should not be released yet, no
  // matter whether we have a wait here or how long the wait is. An arbitrary
  // time period is added here so that if the event mistakenly happens, it is
  // more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));
  // Release() should not have been called as resource is used by last frame.
  ASSERT_EQ(release_call_count, 0);

  // Submit frame without resource. This should cause buffer to be released.
  shell_surface->SubmitCompositorFrameForTesting(CreateCompositorFrame(
      shell_surface.get(), gfx::Rect(buffer_size), gfx::Rect(buffer_size), {}));

  run_loop.Run();
  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

class TestLayerTreeFrameSinkHolder : public LayerTreeFrameSinkHolder {
 public:
  TestLayerTreeFrameSinkHolder(
      SurfaceTreeHost* surface_tree_host,
      std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink> frame_sink)
      : LayerTreeFrameSinkHolder(surface_tree_host, std::move(frame_sink)) {}
  ~TestLayerTreeFrameSinkHolder() override = default;

  using PreReclaimCallback =
      base::RepeatingCallback<void(const std::vector<viz::ReturnedResource>&)>;
  void set_pre_reclaim_callback(PreReclaimCallback callback) {
    pre_reclaim_callback_ = std::move(callback);
  }

  void set_post_reclaim_callback(base::RepeatingClosure callback) {
    post_reclaim_callback_ = std::move(callback);
  }

  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    if (pre_reclaim_callback_) {
      pre_reclaim_callback_.Run(resources);
    }

    LayerTreeFrameSinkHolder::ReclaimResources(std::move(resources));

    if (post_reclaim_callback_) {
      post_reclaim_callback_.Run();
    }
  }

 private:
  PreReclaimCallback pre_reclaim_callback_;
  base::RepeatingClosure post_reclaim_callback_;
};

TEST_F(BufferTest, SurfaceTreeHostNotReclaimCachedFrameResources) {
  gfx::Size buffer_size(256, 256);

  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).SetNoCommit().BuildShellSurface();
  test::SetLayerTreeFrameSinkHolderFactory<TestLayerTreeFrameSinkHolder>(
      shell_surface.get());
  shell_surface->root_surface()->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  TestLayerTreeFrameSinkHolder* frame_sink_holder =
      static_cast<TestLayerTreeFrameSinkHolder*>(
          shell_surface->layer_tree_frame_sink_holder());

  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop1;

  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_call_count, run_loop1.QuitClosure()));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  // Submit frame with `resource`.
  shell_surface->SubmitCompositorFrameForTesting(
      CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                            gfx::Rect(buffer_size), {resource.value()}));
  test::WaitForLastFrameAck(shell_surface.get());

  base::RunLoop run_loop2;
  // Set a callback that will be called when the remote side notify
  // ReclaimResources for `resource`.
  frame_sink_holder->set_pre_reclaim_callback(base::BindLambdaForTesting(
      [&](const std::vector<viz::ReturnedResource>& resources) {
        // Skip if it is not a notification for reclaiming `resource`.
        if (!base::Contains(
                resources, resource->id,
                [](const viz::ReturnedResource& r) { return r.id; })) {
          return;
        }

        run_loop2.Quit();
        // Make sure that this callback is only used once.
        frame_sink_holder->set_pre_reclaim_callback({});

        frame_sink_holder->ClearPendingBeginFramesForTesting();
        // Cause a frame with `resource` is cached. This should hold off
        // reclaming `resource`.
        shell_surface->SubmitCompositorFrameForTesting(
            CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                                  gfx::Rect(buffer_size), {resource.value()}));
      }));

  // Submit a new frame without resource to cause the remote side to stop using
  // `resource` and notify ReclaimResources. The callback set by
  // set_pre_reclaim_callback() above should be run as a result.
  shell_surface->SubmitCompositorFrameForTesting(CreateCompositorFrame(
      shell_surface.get(), gfx::Rect(buffer_size), gfx::Rect(buffer_size), {}));
  run_loop2.Run();

  // Ensure the cached frame is submitted.
  test::WaitForLastFrameAck(shell_surface.get());

  // We expect that Release() is not called, no matter whether we have a wait
  // here or how long the wait is. An arbitrary time period is added here so
  // that if the event mistakenly happens, it is more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Release() should not have been called.
  ASSERT_EQ(release_call_count, 0);

  buffer->OnDetach();

  // Submit a new frame without resource. This will cause buffer to be released.
  shell_surface->SubmitCompositorFrameForTesting(CreateCompositorFrame(
      shell_surface.get(), gfx::Rect(buffer_size), gfx::Rect(buffer_size), {}));

  run_loop1.Run();
  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, SurfaceTreeHostDiscardFrameNotReclaimNewFrameResources) {
  gfx::Size buffer_size(256, 256);

  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();
  test::WaitForLastFrameAck(shell_surface.get());

  LayerTreeFrameSinkHolder* frame_sink_holder =
      shell_surface->layer_tree_frame_sink_holder();

  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop;

  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_call_count, run_loop.QuitClosure()));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  frame_sink_holder->ClearPendingBeginFramesForTesting();

  // Submit a frame with `resource`, which will be cached.
  shell_surface->SubmitCompositorFrameForTesting(
      CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                            gfx::Rect(buffer_size), {resource.value()}));

  // Submit another frame with `resource`. It will cause the previously cached
  // frame to be evicted.
  shell_surface->SubmitCompositorFrameForTesting(
      CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                            gfx::Rect(buffer_size), {resource.value()}));

  buffer->OnDetach();

  // Ensure the cached frame is submitted.
  test::WaitForLastFrameAck(shell_surface.get());

  // We expect that Release() is not called, no matter whether we have a wait
  // here or how long the wait is. An arbitrary time period is added here so
  // that if the event mistakenly happens, it is more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Release() should not have been called.
  ASSERT_EQ(release_call_count, 0);

  // Submit another frame without resource.
  shell_surface->SubmitCompositorFrameForTesting(CreateCompositorFrame(
      shell_surface.get(), gfx::Rect(buffer_size), gfx::Rect(buffer_size), {}));
  test::WaitForLastFrameAck(shell_surface.get());

  run_loop.Run();

  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_F(BufferTest, SurfaceTreeHostDiscardFrameNotReclaimInUseResources) {
  gfx::Size buffer_size(256, 256);

  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).SetNoCommit().BuildShellSurface();
  test::SetLayerTreeFrameSinkHolderFactory<TestLayerTreeFrameSinkHolder>(
      shell_surface.get());
  shell_surface->root_surface()->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  TestLayerTreeFrameSinkHolder* frame_sink_holder =
      static_cast<TestLayerTreeFrameSinkHolder*>(
          shell_surface->layer_tree_frame_sink_holder());

  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop1;

  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_call_count, run_loop1.QuitClosure()));

  buffer->OnAttach();
  // Produce a transferable resource for the contents of the buffer.
  std::optional<viz::TransferableResource> resource =
      buffer->ProduceTransferableResource(
          frame_sink_holder->resource_manager(), nullptr, false,
          gfx::ColorSpace::CreateSRGB(), nullptr);
  ASSERT_TRUE(resource);

  // Submit frame with `resource`.
  shell_surface->SubmitCompositorFrameForTesting(
      CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                            gfx::Rect(buffer_size), {resource.value()}));
  test::WaitForLastFrameAck(shell_surface.get());

  base::RunLoop run_loop2;
  // Set a callback that will be called when the remote side notify
  // ReclaimResources for `resource`.
  frame_sink_holder->set_pre_reclaim_callback(base::BindLambdaForTesting(
      [&](const std::vector<viz::ReturnedResource>& resources) {
        // Skip if it is not a notification for reclaiming `resource`.
        if (!base::Contains(
                resources, resource->id,
                [](const viz::ReturnedResource& r) { return r.id; })) {
          return;
        }

        run_loop2.Quit();
        // Make sure that this callback is only used once.
        frame_sink_holder->set_pre_reclaim_callback({});

        // The evicted cached frame with `resource` shouldn't have caused
        // Release() to be called.
        ASSERT_EQ(release_call_count, 0);
      }));

  frame_sink_holder->ClearPendingBeginFramesForTesting();

  // Cause a frame with `resource` is cached.
  shell_surface->SubmitCompositorFrameForTesting(
      CreateCompositorFrame(shell_surface.get(), gfx::Rect(buffer_size),
                            gfx::Rect(buffer_size), {resource.value()}));

  buffer->OnDetach();

  // Submit a new frame without resource to evict the previously cached frame.
  // It shouldn't cause `resource` to be reclaimed because it is still in use at
  // the remote side.
  shell_surface->SubmitCompositorFrameForTesting(CreateCompositorFrame(
      shell_surface.get(), gfx::Rect(buffer_size), gfx::Rect(buffer_size), {}));

  // Wait for the remote site to notify ReclaimResources for `resource`.
  run_loop2.Run();

  run_loop1.Run();

  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

}  // namespace
}  // namespace exo

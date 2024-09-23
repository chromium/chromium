// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display/display_resource_provider_skia.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence_handle.h"

using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace viz {
namespace {

class MockReleaseCallback {
 public:
  MOCK_METHOD2(Released, void(const gpu::SyncToken& token, bool lost));
};

MATCHER_P(SamePtr, ptr_to_expected, "") {
  return arg.get() == ptr_to_expected;
}

static void CollectResources(std::vector<ReturnedResource>* array,
                             std::vector<ReturnedResource> returned) {
  array->insert(array->end(), std::make_move_iterator(returned.begin()),
                std::make_move_iterator(returned.end()));
}

class MockExternalUseClient : public ExternalUseClient {
 public:
  MockExternalUseClient() = default;
  MOCK_METHOD1(ReleaseImageContexts,
               gpu::SyncToken(
                   std::vector<std::unique_ptr<ImageContext>> image_contexts));
  MOCK_METHOD7(
      CreateImageContext,
      std::unique_ptr<ImageContext>(const gpu::MailboxHolder&,
                                    const gfx::Size&,
                                    SharedImageFormat,
                                    bool,
                                    const std::optional<gpu::VulkanYCbCrInfo>&,
                                    sk_sp<SkColorSpace>,
                                    bool));
};

class DisplayResourceProviderSkiaTest : public testing::Test {
 public:
  DisplayResourceProviderSkiaTest() {
    child_context_provider_ = TestContextProvider::Create();
    child_context_provider_->BindToCurrentSequence();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
  }

  ~DisplayResourceProviderSkiaTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  void SetUp() override {
    resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();
    lock_set_.emplace(resource_provider_.get(), &client_);
  }

  void TearDown() override {
    resource_provider_.reset();
    lock_set_.reset();
  }

  static ReturnCallback GetReturnCallback(
      std::vector<ReturnedResource>* array) {
    return base::BindRepeating(&CollectResources, array);
  }

  TransferableResource CreateResource() {
    constexpr gfx::Size size(64, 64);
    gpu::Mailbox gpu_mailbox = gpu::Mailbox::Generate();
    gpu::SyncToken sync_token = GenSyncToken();
    EXPECT_TRUE(sync_token.HasData());

    TransferableResource gl_resource = TransferableResource::MakeGpu(
        gpu_mailbox, GL_TEXTURE_2D, sync_token, size,
        SinglePlaneFormat::kRGBA_8888, false /* is_overlay_candidate */);
    return gl_resource;
  }

  gpu::SyncToken GenSyncToken() {
    gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                              gpu::CommandBufferId::FromUnsafeValue(0x123),
                              next_fence_sync_++);
    sync_token.SetVerifyFlush();
    return sync_token;
  }

 protected:
  uint64_t next_fence_sync_ = 1;
  scoped_refptr<TestContextProvider> child_context_provider_;
  std::unique_ptr<DisplayResourceProviderSkia> resource_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  testing::NiceMock<MockExternalUseClient> client_;
  std::optional<DisplayResourceProviderSkia::LockSetForExternalUse> lock_set_;
};

TEST_F(DisplayResourceProviderSkiaTest, LockForExternalUse) {
  gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x123),
                             0x42);
  auto mailbox = gpu::Mailbox::Generate();
  constexpr gfx::Size size(64, 64);
  TransferableResource gl_resource = TransferableResource::MakeGpu(
      mailbox, GL_TEXTURE_2D, sync_token1, size, SinglePlaneFormat::kRGBA_8888,
      false /* is_overlay_candidate */);
  ResourceId id1 =
      child_resource_provider_->ImportResource(gl_resource, base::DoNothing());
  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), SurfaceId());

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(
      {id1}, &list,
      static_cast<RasterContextProvider*>(child_context_provider_.get()));
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  ResourceId parent_id = resource_map[list.front().id];

  auto format = SinglePlaneFormat::kRGBA_8888;
  auto owned_image_context = std::make_unique<ExternalUseClient::ImageContext>(
      gpu::MailboxHolder(mailbox, sync_token1, GL_TEXTURE_2D), size, format,
      /*ycbcr_info=*/std::nullopt, /*color_space=*/nullptr);
  auto* image_context = owned_image_context.get();

  gpu::MailboxHolder holder;
  EXPECT_CALL(client_, CreateImageContext(_, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<0>(&holder),
                      Return(ByMove(std::move(owned_image_context)))));

  ExternalUseClient::ImageContext* locked_image_context =
      lock_set_->LockResource(parent_id, /*maybe_concurrent_reads=*/true,
                              /*is_video_plane=*/false);
  EXPECT_EQ(image_context, locked_image_context);
  ASSERT_EQ(holder.mailbox, mailbox);
  ASSERT_TRUE(holder.sync_token.HasData());

  // Don't release while locked.
  EXPECT_CALL(client_, ReleaseImageContexts(_)).Times(0);
  // Return the resources back to the child. Nothing should happen because
  // of the resource lock.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  // The resource should not be returned due to the external use lock.
  EXPECT_EQ(0u, returned_to_child.size());

  gpu::SyncToken sync_token2(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x234),
                             0x456);
  sync_token2.SetVerifyFlush();

  gpu::SyncToken sync_token3(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x234),
                             0x567);
  sync_token3.SetVerifyFlush();
  // We will get a second release of |parent_id| now that we've released our
  // external lock.
  EXPECT_CALL(client_, ReleaseImageContexts(
                           testing::ElementsAre(SamePtr(locked_image_context))))
      .WillOnce(Return(sync_token3));
  // UnlockResources will also call DeclareUsedResourcesFromChild.
  lock_set_->UnlockResources(sync_token2);
  // The resource should be returned after the lock is released.
  EXPECT_EQ(1u, returned_to_child.size());
  EXPECT_EQ(sync_token3, returned_to_child[0].sync_token);
  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));
  child_resource_provider_->RemoveImportedResource(id1);
}

TEST_F(DisplayResourceProviderSkiaTest, LockForExternalUseWebView) {
  gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x123),
                             0x42);
  auto mailbox = gpu::Mailbox::Generate();
  constexpr gfx::Size size(64, 64);
  TransferableResource gl_resource = TransferableResource::MakeGpu(
      mailbox, GL_TEXTURE_2D, sync_token1, size, SinglePlaneFormat::kRGBA_8888,
      false /* is_overlay_candidate */);
  ResourceId id1 =
      child_resource_provider_->ImportResource(gl_resource, base::DoNothing());
  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), SurfaceId());

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(
      {id1}, &list,
      static_cast<RasterContextProvider*>(child_context_provider_.get()));
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  ResourceId parent_id = resource_map[list.front().id];

  auto format = SinglePlaneFormat::kRGBA_8888;
  auto owned_image_context = std::make_unique<ExternalUseClient::ImageContext>(
      gpu::MailboxHolder(mailbox, sync_token1, GL_TEXTURE_2D), size, format,
      /*ycbcr_info=*/std::nullopt, /*color_space=*/nullptr);
  auto* image_context = owned_image_context.get();

  gpu::MailboxHolder holder;
  EXPECT_CALL(client_, CreateImageContext(_, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<0>(&holder),
                      Return(ByMove(std::move(owned_image_context)))));

  ExternalUseClient::ImageContext* locked_image_context =
      lock_set_->LockResource(parent_id, /*maybe_concurrent_reads=*/true,
                              /*is_video_plane=*/false);
  EXPECT_EQ(image_context, locked_image_context);
  ASSERT_EQ(holder.mailbox, mailbox);
  ASSERT_TRUE(holder.sync_token.HasData());

  // Don't release while locked.
  EXPECT_CALL(client_, ReleaseImageContexts(_)).Times(0);
  // Return the resources back to the child. Nothing should happen because
  // of the resource lock.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  // The resource should not be returned due to the external use lock.
  EXPECT_EQ(0u, returned_to_child.size());

  // Disable access to gpu thread.
  resource_provider_->SetAllowAccessToGPUThread(false);

  gpu::SyncToken sync_token2(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x234),
                             0x456);
  sync_token2.SetVerifyFlush();

  gpu::SyncToken sync_token3(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x234),
                             0x567);
  sync_token3.SetVerifyFlush();

  // Without GPU thread access no ReleaseImageContexts() should happen
  EXPECT_CALL(client_, ReleaseImageContexts(_)).Times(0);
  // Unlock resources
  lock_set_->UnlockResources(sync_token2);
  // Resources should not be returned because we can't unlock them on GPU
  // thread.
  EXPECT_EQ(0u, returned_to_child.size());

  // We will get a second release of |parent_id| now that we've released our
  // external lock and have access to GPU thread.
  EXPECT_CALL(client_, ReleaseImageContexts(
                           testing::ElementsAre(SamePtr(locked_image_context))))
      .WillOnce(Return(sync_token3));
  // Enable access to GPU Thread
  resource_provider_->SetAllowAccessToGPUThread(true);

  // The resource should be returned after the lock is released.
  EXPECT_EQ(1u, returned_to_child.size());
  EXPECT_EQ(sync_token3, returned_to_child[0].sync_token);
  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));
  child_resource_provider_->RemoveImportedResource(id1);
}

class TestGpuCommandsCompletedFence : public ResourceFence {
 public:
  explicit TestGpuCommandsCompletedFence(
      DisplayResourceProviderSkia* resource_provider)
      : ResourceFence(resource_provider) {}

  // ResourceFence implementation.
  bool HasPassed() override { return passed_; }
  gfx::GpuFenceHandle GetGpuFenceHandle() override {
    NOTREACHED_IN_MIGRATION();
    return gfx::GpuFenceHandle();
  }

  void Signal() {
    passed_ = true;
    FencePassed();
  }

 private:
  ~TestGpuCommandsCompletedFence() override = default;

  bool passed_ = false;
  base::WeakPtr<DisplayResourceProvider> resource_provider_;
};

class TestReleaseFence : public ResourceFence {
 public:
  explicit TestReleaseFence(DisplayResourceProviderSkia* resource_provider)
      : ResourceFence(resource_provider) {}

  // ResourceFence implementation.
  bool HasPassed() override { return release_fence_.has_value(); }
  gfx::GpuFenceHandle GetGpuFenceHandle() override {
    return HasPassed() ? release_fence_->Clone() : gfx::GpuFenceHandle();
  }

  void SetReleaseFence(gfx::GpuFenceHandle release_fence) {
    release_fence_ = std::move(release_fence);
    FencePassed();
  }

 private:
  ~TestReleaseFence() override = default;

  std::optional<gfx::GpuFenceHandle> release_fence_;
  base::WeakPtr<DisplayResourceProvider> resource_provider_;
};

TEST_F(DisplayResourceProviderSkiaTest,
       ResourceFenceStopsReturnToChildOrDelete) {
  const std::vector<TransferableResource::SynchronizationType>
      kSynchronizationTypes = {
          TransferableResource::SynchronizationType::kGpuCommandsCompleted,
          TransferableResource::SynchronizationType::kReleaseFence};
  for (auto sync_type : kSynchronizationTypes) {
    MockReleaseCallback release;

    TransferableResource tran1 = CreateResource();
    tran1.synchronization_type = sync_type;
    ResourceId id1 = child_resource_provider_->ImportResource(
        tran1, base::BindOnce(&MockReleaseCallback::Released,
                              base::Unretained(&release)));

    TransferableResource tran2 = CreateResource();
    ASSERT_EQ(tran2.synchronization_type,
              TransferableResource::SynchronizationType::kSyncToken);
    ResourceId id2 = child_resource_provider_->ImportResource(
        tran2, base::BindOnce(&MockReleaseCallback::Released,
                              base::Unretained(&release)));

    std::vector<ReturnedResource> returned_to_child;
    int child_id = resource_provider_->CreateChild(
        GetReturnCallback(&returned_to_child), SurfaceId());

    // Transfer resources to the parent.
    std::vector<TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        {id1, id2}, &list,
        static_cast<RasterContextProvider*>(child_context_provider_.get()));
    ASSERT_EQ(2u, list.size());
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

    resource_provider_->ReceiveFromChild(child_id, list);

    // In DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        resource_provider_->GetChildToParentMap(child_id);

    scoped_refptr<ResourceFence> fence;
    TestGpuCommandsCompletedFence* gpu_commands_completed_fence = nullptr;
    TestReleaseFence* release_fence = nullptr;
    if (sync_type ==
        TransferableResource::SynchronizationType::kGpuCommandsCompleted) {
      fence = base::MakeRefCounted<TestGpuCommandsCompletedFence>(
          resource_provider_.get());
      gpu_commands_completed_fence =
          static_cast<TestGpuCommandsCompletedFence*>(fence.get());
      resource_provider_->SetGpuCommandsCompletedFence(fence.get());
    } else {
      ASSERT_EQ(TransferableResource::SynchronizationType::kReleaseFence,
                sync_type);
      fence = base::MakeRefCounted<TestReleaseFence>(resource_provider_.get());
      release_fence = static_cast<TestReleaseFence*>(fence.get());
      resource_provider_->SetReleaseFence(fence.get());
    }

    {
      for (auto& resource : list) {
        ResourceId parent_id = resource_map[resource.id];
        lock_set_->LockResource(parent_id, /*maybe_concurrent_reads=*/true,
                                /*is_video_plane=*/false);
      }
      lock_set_->UnlockResources(GenSyncToken());
    }
    EXPECT_EQ(0u, returned_to_child.size());
    EXPECT_EQ(2u, resource_provider_->num_resources());

    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      ResourceIdSet());

    EXPECT_EQ(1u, resource_provider_->num_resources());
    EXPECT_EQ(1u, returned_to_child.size());

    // Signalling the resource fence should return the resources automatically.
    if (gpu_commands_completed_fence) {
      gpu_commands_completed_fence->Signal();
    } else {
      gfx::GpuFenceHandle fake_handle;
#if BUILDFLAG(IS_POSIX)
      const int32_t kFenceFd = dup(1);
      fake_handle.Adopt(base::ScopedFD(kFenceFd));
#endif
      release_fence->SetReleaseFence(std::move(fake_handle));
    }

    EXPECT_EQ(0u, resource_provider_->num_resources());
    EXPECT_EQ(2u, returned_to_child.size());

#if BUILDFLAG(IS_POSIX)
    EXPECT_TRUE(returned_to_child[0].release_fence.is_null());
    if (release_fence)
      EXPECT_FALSE(returned_to_child[1].release_fence.is_null());
    else
      EXPECT_TRUE(returned_to_child[1].release_fence.is_null());
#endif

    child_resource_provider_->ReceiveReturnsFromParent(
        std::move(returned_to_child));
    EXPECT_CALL(release, Released(_, _)).Times(2);
    child_resource_provider_->RemoveImportedResource(id1);
    child_resource_provider_->RemoveImportedResource(id2);
  }
}

TEST_F(DisplayResourceProviderSkiaTest, ResourceFenceDestroyChild) {
  const std::vector<TransferableResource::SynchronizationType>
      kSynchronizationTypes = {
          TransferableResource::SynchronizationType::kGpuCommandsCompleted,
          TransferableResource::SynchronizationType::kReleaseFence};
  for (auto sync_type : kSynchronizationTypes) {
    MockReleaseCallback release;

    TransferableResource tran1 = CreateResource();
    tran1.synchronization_type = sync_type;
    ResourceId id1 = child_resource_provider_->ImportResource(
        tran1, base::BindOnce(&MockReleaseCallback::Released,
                              base::Unretained(&release)));

    TransferableResource tran2 = CreateResource();
    ASSERT_EQ(tran2.synchronization_type,
              TransferableResource::SynchronizationType::kSyncToken);
    ResourceId id2 = child_resource_provider_->ImportResource(
        tran2, base::BindOnce(&MockReleaseCallback::Released,
                              base::Unretained(&release)));

    std::vector<ReturnedResource> returned_to_child;
    int child_id = resource_provider_->CreateChild(
        GetReturnCallback(&returned_to_child), SurfaceId());

    // Transfer resources to the parent.
    std::vector<TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        {id1, id2}, &list,
        static_cast<RasterContextProvider*>(child_context_provider_.get()));
    ASSERT_EQ(2u, list.size());
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

    resource_provider_->ReceiveFromChild(child_id, list);

    // In DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        resource_provider_->GetChildToParentMap(child_id);

    scoped_refptr<ResourceFence> fence;
    TestGpuCommandsCompletedFence* gpu_commands_completed_fence = nullptr;
    TestReleaseFence* release_fence = nullptr;
    if (sync_type ==
        TransferableResource::SynchronizationType::kGpuCommandsCompleted) {
      fence = base::MakeRefCounted<TestGpuCommandsCompletedFence>(
          resource_provider_.get());
      gpu_commands_completed_fence =
          static_cast<TestGpuCommandsCompletedFence*>(fence.get());
      resource_provider_->SetGpuCommandsCompletedFence(fence.get());
    } else {
      ASSERT_EQ(TransferableResource::SynchronizationType::kReleaseFence,
                sync_type);
      fence = base::MakeRefCounted<TestReleaseFence>(resource_provider_.get());
      release_fence = static_cast<TestReleaseFence*>(fence.get());
      resource_provider_->SetReleaseFence(fence.get());
    }

    {
      for (auto& resource : list) {
        ResourceId parent_id = resource_map[resource.id];
        lock_set_->LockResource(parent_id, /*maybe_concurrent_reads=*/true,
                                /*is_video_plane=*/false);
      }
      lock_set_->UnlockResources(GenSyncToken());
    }
    EXPECT_EQ(0u, returned_to_child.size());
    EXPECT_EQ(2u, resource_provider_->num_resources());

    resource_provider_->DestroyChild(child_id);

    EXPECT_EQ(0u, resource_provider_->num_resources());
    EXPECT_EQ(2u, returned_to_child.size());

    // id1 should be lost and id2 should not.
    EXPECT_EQ(returned_to_child[0].lost, returned_to_child[0].id == id1);
    EXPECT_EQ(returned_to_child[1].lost, returned_to_child[1].id == id1);

    // fence signalling should be noop.
    if (gpu_commands_completed_fence) {
      gpu_commands_completed_fence->Signal();
    } else {
      gfx::GpuFenceHandle fake_handle;
#if BUILDFLAG(IS_POSIX)
      const int32_t kFenceFd = dup(1);
      fake_handle.Adopt(base::ScopedFD(kFenceFd));
#endif
      release_fence->SetReleaseFence(std::move(fake_handle));
    }

    EXPECT_EQ(0u, resource_provider_->num_resources());
    EXPECT_EQ(2u, returned_to_child.size());

    child_resource_provider_->ReceiveReturnsFromParent(
        std::move(returned_to_child));
    EXPECT_CALL(release, Released(_, _)).Times(2);
    child_resource_provider_->RemoveImportedResource(id1);
    child_resource_provider_->RemoveImportedResource(id2);
  }
}

TEST_F(DisplayResourceProviderSkiaTest, ResourceFenceOutlivesResourceProvider) {
  MockReleaseCallback release;

  TransferableResource tran1 = CreateResource();
  tran1.synchronization_type =
      TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran1, base::BindOnce(&MockReleaseCallback::Released,
                            base::Unretained(&release)));

  TransferableResource tran2 = CreateResource();
  tran2.synchronization_type =
      TransferableResource::SynchronizationType::kReleaseFence;
  ResourceId id2 = child_resource_provider_->ImportResource(
      tran2, base::BindOnce(&MockReleaseCallback::Released,
                            base::Unretained(&release)));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), SurfaceId());

  // Transfer resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(
      {id1, id2}, &list,
      static_cast<RasterContextProvider*>(child_context_provider_.get()));
  ASSERT_EQ(2u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestGpuCommandsCompletedFence> gpu_commands_completed_fence =
      base::MakeRefCounted<TestGpuCommandsCompletedFence>(
          resource_provider_.get());
  resource_provider_->SetGpuCommandsCompletedFence(
      gpu_commands_completed_fence.get());

  scoped_refptr<TestReleaseFence> release_fence =
      base::MakeRefCounted<TestReleaseFence>(resource_provider_.get());
  resource_provider_->SetReleaseFence(release_fence.get());

  {
    for (auto& resource : list) {
      ResourceId parent_id = resource_map[resource.id];
      lock_set_->LockResource(parent_id, /*maybe_concurrent_reads=*/true,
                              /*is_video_plane=*/false);
    }
    lock_set_->UnlockResources(GenSyncToken());
  }
  EXPECT_EQ(0u, returned_to_child.size());
  EXPECT_EQ(2u, resource_provider_->num_resources());

  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());

  EXPECT_EQ(0u, returned_to_child.size());
  EXPECT_EQ(2u, resource_provider_->num_resources());

  resource_provider_.reset();
  EXPECT_EQ(2u, returned_to_child.size());

  // Signalling the dangling resource fence should not crash.
  if (gpu_commands_completed_fence) {
    gpu_commands_completed_fence->Signal();
  } else {
    gfx::GpuFenceHandle fake_handle;
#if BUILDFLAG(IS_POSIX)
    const int32_t kFenceFd = dup(1);
    fake_handle.Adopt(base::ScopedFD(kFenceFd));
#endif
    release_fence->SetReleaseFence(std::move(fake_handle));
  }

  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));
  EXPECT_CALL(release, Released(_, _)).Times(2);
  child_resource_provider_->RemoveImportedResource(id1);
  child_resource_provider_->RemoveImportedResource(id2);
}

// Test that ScopedBatchReturnResources batching works.
TEST_F(DisplayResourceProviderSkiaTest,
       ScopedBatchReturnResourcesPreventsReturn) {
  MockReleaseCallback release;

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), SurfaceId());

  // Transfer some resources to the parent.
  constexpr size_t kTotalResources = 5;
  constexpr size_t kLockedResources = 3;
  constexpr size_t kUsedResources = 4;
  ResourceId ids[kTotalResources];
  for (auto& id : ids) {
    TransferableResource tran = CreateResource();
    id = child_resource_provider_->ImportResource(
        tran, base::BindOnce(&MockReleaseCallback::Released,
                             base::Unretained(&release)));
  }
  std::vector<ResourceId> resource_ids_to_transfer(ids, ids + kTotalResources);

  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(
      resource_ids_to_transfer, &list,
      static_cast<RasterContextProvider*>(child_context_provider_.get()));
  ASSERT_EQ(kTotalResources, list.size());
  for (const auto& id : ids)
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  std::vector<
      std::unique_ptr<DisplayResourceProvider::ScopedReadLockSharedImage>>
      read_locks;
  for (size_t i = 0; i < kLockedResources; i++) {
    ResourceId mapped_resource_id = resource_map[ids[i]];
    lock_set_->LockResource(mapped_resource_id, /*maybe_concurrent_reads=*/true,
                            /*is_video_plane=*/false);
  }

  // Mark all locked resources, and one unlocked resource as used for first
  // batch.
  {
    DisplayResourceProvider::ScopedBatchReturnResources returner(
        resource_provider_.get());
    resource_provider_->DeclareUsedResourcesFromChild(
        child_id, ResourceIdSet(ids, ids + kUsedResources));
    EXPECT_EQ(0u, returned_to_child.size());
  }
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));
  returned_to_child.clear();

  // Return all locked resources.
  {
    DisplayResourceProvider::ScopedBatchReturnResources returner(
        resource_provider_.get());
    resource_provider_->DeclareUsedResourcesFromChild(
        child_id, ResourceIdSet(ids + kLockedResources, ids + kUsedResources));
    // Can be called multiple times while batching is enabled.  This happens in
    // practice when the same surface is visited using different paths during
    // surface aggregation.
    resource_provider_->DeclareUsedResourcesFromChild(
        child_id, ResourceIdSet(ids + kLockedResources, ids + kUsedResources));
    lock_set_->UnlockResources(GenSyncToken());
    EXPECT_EQ(0u, returned_to_child.size());
  }
  EXPECT_EQ(kLockedResources, returned_to_child.size());
  // Returned resources that were locked share the same sync token.
  for (const auto& resource : returned_to_child)
    EXPECT_EQ(resource.sync_token, returned_to_child[0].sync_token);

  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));
  returned_to_child.clear();

  // Returns from destroying the child is also batched.
  {
    DisplayResourceProvider::ScopedBatchReturnResources returner(
        resource_provider_.get());
    resource_provider_->DestroyChild(child_id);
    EXPECT_EQ(0u, returned_to_child.size());
  }
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));
  returned_to_child.clear();

  EXPECT_CALL(release, Released(_, _)).Times(kTotalResources);
  for (const auto& id : ids)
    child_resource_provider_->RemoveImportedResource(id);
}

}  // namespace
}  // namespace viz

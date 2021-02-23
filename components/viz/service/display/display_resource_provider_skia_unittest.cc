// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_skia.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/geometry/rect.h"

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
                             const std::vector<ReturnedResource>& returned) {
  array->insert(array->end(), returned.begin(), returned.end());
}

class MockExternalUseClient : public ExternalUseClient {
 public:
  MockExternalUseClient() = default;
  MOCK_METHOD1(ReleaseImageContexts,
               gpu::SyncToken(
                   std::vector<std::unique_ptr<ImageContext>> image_contexts));
  MOCK_METHOD6(CreateImageContext,
               std::unique_ptr<ImageContext>(
                   const gpu::MailboxHolder&,
                   const gfx::Size&,
                   ResourceFormat,
                   bool,
                   const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
                   sk_sp<SkColorSpace>));
};

class DisplayResourceProviderSkiaTest : public testing::Test {
 public:
  DisplayResourceProviderSkiaTest() {
    child_context_provider_ = TestContextProvider::Create();
    child_context_provider_->BindToCurrentThread();

    resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();

    lock_set_.emplace(resource_provider_.get(), &client_);

    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
  }

  ~DisplayResourceProviderSkiaTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  static ReturnCallback GetReturnCallback(
      std::vector<ReturnedResource>* array) {
    return base::BindRepeating(&CollectResources, array);
  }

  TransferableResource CreateResource(ResourceFormat format) {
    constexpr gfx::Size size(64, 64);
    gpu::Mailbox gpu_mailbox = gpu::Mailbox::GenerateForSharedImage();
    gpu::SyncToken sync_token = GenSyncToken();
    EXPECT_TRUE(sync_token.HasData());

    TransferableResource gl_resource = TransferableResource::MakeGL(
        gpu_mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token, size,
        false /* is_overlay_candidate */);
    gl_resource.format = format;
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
  base::Optional<DisplayResourceProviderSkia::LockSetForExternalUse> lock_set_;
};

TEST_F(DisplayResourceProviderSkiaTest, LockForExternalUse) {
  gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x123),
                             0x42);
  auto mailbox = gpu::Mailbox::Generate();
  constexpr gfx::Size size(64, 64);
  TransferableResource gl_resource = TransferableResource::MakeGL(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token1, size,
      false /* is_overlay_candidate */);
  ResourceId id1 = child_resource_provider_->ImportResource(
      gl_resource, SingleReleaseCallback::Create(base::DoNothing()));
  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

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

  auto owned_image_context = std::make_unique<ExternalUseClient::ImageContext>(
      gpu::MailboxHolder(mailbox, sync_token1, GL_TEXTURE_2D), size, RGBA_8888,
      /*ycbcr_info=*/base::nullopt, /*color_space=*/nullptr);
  auto* image_context = owned_image_context.get();

  gpu::MailboxHolder holder;
  EXPECT_CALL(client_, CreateImageContext(_, _, _, _, _, _))
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
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
}

TEST_F(DisplayResourceProviderSkiaTest, LockForExternalUseWebView) {
  gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x123),
                             0x42);
  auto mailbox = gpu::Mailbox::Generate();
  constexpr gfx::Size size(64, 64);
  TransferableResource gl_resource = TransferableResource::MakeGL(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token1, size,
      false /* is_overlay_candidate */);
  ResourceId id1 = child_resource_provider_->ImportResource(
      gl_resource, SingleReleaseCallback::Create(base::DoNothing()));
  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

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

  auto owned_image_context = std::make_unique<ExternalUseClient::ImageContext>(
      gpu::MailboxHolder(mailbox, sync_token1, GL_TEXTURE_2D), size, RGBA_8888,
      /*ycbcr_info=*/base::nullopt, /*color_space=*/nullptr);
  auto* image_context = owned_image_context.get();

  gpu::MailboxHolder holder;
  EXPECT_CALL(client_, CreateImageContext(_, _, _, _, _, _))
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
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
}

class TestFence : public ResourceFence {
 public:
  TestFence() = default;

  // ResourceFence implementation.
  void Set() override {}
  bool HasPassed() override { return passed; }

  bool passed = false;

 private:
  ~TestFence() override = default;
};

TEST_F(DisplayResourceProviderSkiaTest,
       ReadLockFenceStopsReturnToChildOrDelete) {
  MockReleaseCallback release;
  TransferableResource tran1 = CreateResource(RGBA_8888);
  tran1.read_lock_fences_enabled = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran1, SingleReleaseCallback::Create(base::BindOnce(
                 &MockReleaseCallback::Released, base::Unretained(&release))));

  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(
      {id1}, &list,
      static_cast<RasterContextProvider*>(child_context_provider_.get()));
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(list[0].read_lock_fences_enabled);

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    ResourceId parent_id = resource_map[list.front().id];
    lock_set_->LockResource(parent_id, /*maybe_concurrent_reads=*/true,
                            /*is_video_plane=*/false);
    lock_set_->UnlockResources(GenSyncToken());
  }
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(0u, returned_to_child.size());

  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(0u, returned_to_child.size());
  fence->passed = true;

  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  EXPECT_CALL(release, Released(_, _));
  child_resource_provider_->RemoveImportedResource(id1);
}

TEST_F(DisplayResourceProviderSkiaTest, ReadLockFenceDestroyChild) {
  MockReleaseCallback release;

  TransferableResource tran1 = CreateResource(RGBA_8888);
  tran1.read_lock_fences_enabled = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran1, SingleReleaseCallback::Create(base::BindOnce(
                 &MockReleaseCallback::Released, base::Unretained(&release))));

  TransferableResource tran2 = CreateResource(RGBA_8888);
  tran2.read_lock_fences_enabled = false;
  ResourceId id2 = child_resource_provider_->ImportResource(
      tran2, SingleReleaseCallback::Create(base::BindOnce(
                 &MockReleaseCallback::Released, base::Unretained(&release))));

  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

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

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
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

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  EXPECT_CALL(release, Released(_, _)).Times(2);
  child_resource_provider_->RemoveImportedResource(id1);
  child_resource_provider_->RemoveImportedResource(id2);
}

TEST_F(DisplayResourceProviderSkiaTest, ReadLockFenceContextLost) {
  TransferableResource tran1 = CreateResource(RGBA_8888);
  tran1.read_lock_fences_enabled = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran1, SingleReleaseCallback::Create(base::DoNothing()));

  TransferableResource tran2 = CreateResource(RGBA_8888);
  tran2.read_lock_fences_enabled = false;
  ResourceId id2 = child_resource_provider_->ImportResource(
      tran2, SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

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

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    for (auto& resource : list) {
      ResourceId parent_id = resource_map[resource.id];
      DisplayResourceProvider::ScopedReadLockSharedImage lock(
          resource_provider_.get(), parent_id);
    }
  }
  EXPECT_EQ(0u, returned_to_child.size());

  EXPECT_EQ(2u, resource_provider_->num_resources());
  resource_provider_->DidLoseContextProvider();
  resource_provider_ = nullptr;

  EXPECT_EQ(2u, returned_to_child.size());

  EXPECT_TRUE(returned_to_child[0].lost);
  EXPECT_TRUE(returned_to_child[1].lost);

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
  child_resource_provider_->RemoveImportedResource(id2);
}

// Test that ScopedBatchReturnResources batching works.
TEST_F(DisplayResourceProviderSkiaTest,
       ScopedBatchReturnResourcesPreventsReturn) {
  MockReleaseCallback release;

  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer some resources to the parent.
  constexpr size_t kTotalResources = 5;
  constexpr size_t kLockedResources = 3;
  constexpr size_t kUsedResources = 4;
  ResourceId ids[kTotalResources];
  for (auto& id : ids) {
    TransferableResource tran = CreateResource(RGBA_8888);
    id = child_resource_provider_->ImportResource(
        tran, SingleReleaseCallback::Create(base::BindOnce(
                  &MockReleaseCallback::Released, base::Unretained(&release))));
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
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
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

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  returned_to_child.clear();

  // Returns from destroying the child is also batched.
  {
    DisplayResourceProvider::ScopedBatchReturnResources returner(
        resource_provider_.get());
    resource_provider_->DestroyChild(child_id);
    EXPECT_EQ(0u, returned_to_child.size());
  }
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  returned_to_child.clear();

  EXPECT_CALL(release, Released(_, _)).Times(kTotalResources);
  for (const auto& id : ids)
    child_resource_provider_->RemoveImportedResource(id);
}  // namespace

TEST_F(DisplayResourceProviderSkiaTest, LostMailboxInParent) {
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  auto tran = CreateResource(RGBA_8888);
  tran.id = ResourceId(11);

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child));

  // Receive a resource then lose the gpu context.
  resource_provider_->ReceiveFromChild(child_id, {tran});
  resource_provider_->DidLoseContextProvider();

  // Transfer resources back from the parent to the child.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, {});
  ASSERT_EQ(1u, returned_to_child.size());

  // Losing an output surface only loses hardware resources.
  EXPECT_EQ(returned_to_child[0].lost, true);
}

#if defined(OS_ANDROID)
TEST_F(DisplayResourceProviderSkiaTest, OverlayPromotionHint) {
  gpu::Mailbox external_mailbox = gpu::Mailbox::GenerateForSharedImage();
  gpu::SyncToken external_sync_token = GenSyncToken();
  EXPECT_TRUE(external_sync_token.HasData());

  TransferableResource id1_transfer = TransferableResource::MakeGL(
      external_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES, external_sync_token,
      gfx::Size(1, 1), true);
  id1_transfer.wants_promotion_hint = true;
  id1_transfer.is_backed_by_surface_texture = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      id1_transfer, SingleReleaseCallback::Create(base::DoNothing()));

  TransferableResource id2_transfer = TransferableResource::MakeGL(
      external_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES, external_sync_token,
      gfx::Size(1, 1), true);
  id2_transfer.wants_promotion_hint = false;
  id2_transfer.is_backed_by_surface_texture = false;
  ResourceId id2 = child_resource_provider_->ImportResource(
      id2_transfer, SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(
      {id1, id2}, &list,
      static_cast<RasterContextProvider*>(child_context_provider_.get()));
  ASSERT_EQ(2u, list.size());
  resource_provider_->ReceiveFromChild(child_id, list);
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceId mapped_id1 = resource_map[list[0].id];
  ResourceId mapped_id2 = resource_map[list[1].id];

  // The promotion hints should not be recorded until after we wait.  This is
  // because we can't notify them until they're synchronized, and we choose to
  // ignore unwaited resources rather than send them a "no" hint.  If they end
  // up in the request set before we wait, then the attempt to notify them wil;
  // DCHECK when we try to lock them for reading in SendPromotionHints.
  EXPECT_EQ(0u, resource_provider_->CountPromotionHintRequestsForTesting());
  EXPECT_FALSE(resource_provider_->DoAnyResourcesWantPromotionHints());
  {
    resource_provider_->InitializePromotionHintRequest(mapped_id1);
    DisplayResourceProvider::ScopedReadLockSharedImage lock(
        resource_provider_.get(), mapped_id1);
  }
  EXPECT_EQ(1u, resource_provider_->CountPromotionHintRequestsForTesting());
  EXPECT_TRUE(resource_provider_->DoAnyResourcesWantPromotionHints());

  ResourceIdSet resource_ids_to_receive;
  resource_ids_to_receive.insert(id1);
  resource_ids_to_receive.insert(id2);
  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    resource_ids_to_receive);

  EXPECT_EQ(2u, resource_provider_->num_resources());

  EXPECT_NE(kInvalidResourceId, mapped_id1);
  EXPECT_NE(kInvalidResourceId, mapped_id2);

  // Make sure that the request for a promotion hint was noticed.
  EXPECT_TRUE(resource_provider_->IsOverlayCandidate(mapped_id1));
  EXPECT_TRUE(resource_provider_->IsBackedBySurfaceTexture(mapped_id1));
  EXPECT_TRUE(resource_provider_->DoesResourceWantPromotionHint(mapped_id1));

  EXPECT_TRUE(resource_provider_->IsOverlayCandidate(mapped_id2));
  EXPECT_FALSE(resource_provider_->IsBackedBySurfaceTexture(mapped_id2));
  EXPECT_FALSE(resource_provider_->DoesResourceWantPromotionHint(mapped_id2));

  // ResourceProvider maintains a set of promotion hint requests that should be
  // cleared when resources are deleted.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(2u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  EXPECT_EQ(0u, resource_provider_->CountPromotionHintRequestsForTesting());
  EXPECT_FALSE(resource_provider_->DoAnyResourcesWantPromotionHints());

  resource_provider_->DestroyChild(child_id);

  child_resource_provider_->RemoveImportedResource(id2);
  child_resource_provider_->RemoveImportedResource(id1);
}
#endif

}  // namespace
}  // namespace viz

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/client/client_resource_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/viz_test_suite.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Each;

namespace viz {
namespace {

// Mocks ResourceFlushCallback
class MockFlushCallback {
 public:
  MOCK_METHOD0(FlushCallback, void());
};

class ClientResourceProviderTest : public testing::TestWithParam<bool> {
 protected:
  ClientResourceProviderTest()
      : use_gpu_(GetParam()),
        context_provider_(TestContextProvider::Create()),
        bound_(context_provider_->BindToCurrentSequence()) {
    DCHECK_EQ(bound_, gpu::ContextResult::kSuccess);
  }

  // Some tests want to override feature flags that are checked at construction
  // time. Allow them to recreate the provider.
  void InitProvider() {
    // VizTestSuite::task_environment is set for us. We use the default
    // TaskRunner for the Main thread. We then create a second TaskRunner which
    // is built on a separate thread for the Compositor thread.
    provider_ = std::make_unique<ClientResourceProvider>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::ThreadPool::CreateSequencedTaskRunner({}),
        base::BindRepeating(&MockFlushCallback::FlushCallback,
                            base::Unretained(&mock_flush_callback_)));
  }

  void SetUp() override { InitProvider(); }

  void TearDown() override { provider_ = nullptr; }

  gpu::Mailbox MailboxFromChar(char value) {
    gpu::Mailbox mailbox;
    memset(mailbox.name, value, sizeof(mailbox.name));
    return mailbox;
  }

  gpu::SyncToken SyncTokenFromUInt(uint32_t value) {
    return gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                          gpu::CommandBufferId::FromUnsafeValue(0x123), value);
  }

  TransferableResource MakeTransferableResource(bool gpu,
                                                char mailbox_char,
                                                uint32_t sync_token_value) {
    TransferableResource r;
    r.id = ResourceId(mailbox_char);
    r.is_software = !gpu;
    r.size = gfx::Size(10, 11);
    r.set_mailbox(MailboxFromChar(mailbox_char));
    if (gpu) {
      r.set_sync_token(SyncTokenFromUInt(sync_token_value));
      r.set_texture_target(6);
    }
    return r;
  }

  bool use_gpu() const { return use_gpu_; }
  ClientResourceProvider& provider() const { return *provider_; }
  RasterContextProvider* context_provider() const {
    return context_provider_.get();
  }

  void DestroyProvider() {
    provider_->ShutdownAndReleaseAllResources();
    provider_ = nullptr;
  }

  void ExpectFlush() { EXPECT_CALL(mock_flush_callback_, FlushCallback()); }

 private:
  MockFlushCallback mock_flush_callback_;

  bool use_gpu_;
  scoped_refptr<TestContextProvider> context_provider_;
  gpu::ContextResult bound_;
  std::unique_ptr<ClientResourceProvider> provider_;
};

INSTANTIATE_TEST_SUITE_P(ClientResourceProviderTests,
                         ClientResourceProviderTest,
                         ::testing::Values(false, true));

class MockReleaseCallback {
 public:
  MOCK_METHOD2(Released, void(const gpu::SyncToken& token, bool lost));
  MOCK_METHOD3(ReleasedWithId,
               void(ResourceId id, const gpu::SyncToken& token, bool lost));
  MOCK_METHOD0(Evicted, void());
};

TEST_P(ClientResourceProviderTest, TransferableResourceReleased) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));
  // The local id is different.
  EXPECT_NE(id, tran.id);

  // The same SyncToken that was sent is returned when the resource was never
  // exported. The SyncToken may be from any context, and the ReleaseCallback
  // may need to wait on it before interacting with the resource on its context.
  EXPECT_CALL(release, Released(tran.sync_token(), false));
  provider().RemoveImportedResource(id);
}

TEST_P(ClientResourceProviderTest, TransferableResourceSendToParent) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());
  ASSERT_EQ(exported.size(), 1u);

  // Exported resource matches except for the id which was mapped
  // to the local ResourceProvider, and the sync token should be
  // verified if it's a gpu resource.
  gpu::SyncToken verified_sync_token = tran.sync_token();
  if (!tran.is_software)
    verified_sync_token.SetVerifyFlush();
  EXPECT_EQ(exported[0].id, id);
  EXPECT_EQ(exported[0].is_software, tran.is_software);
  EXPECT_EQ(exported[0].size, tran.size);
  EXPECT_EQ(exported[0].mailbox(), tran.mailbox());
  EXPECT_EQ(exported[0].sync_token(), verified_sync_token);
  EXPECT_EQ(exported[0].texture_target(), tran.texture_target());

  // Exported resources are not released when removed, until the export returns.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Return the resource, with a sync token if using gpu.
  std::vector<ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = exported[0].id;
  if (use_gpu())
    returned.back().sync_token = SyncTokenFromUInt(31);
  returned.back().count = 1;
  returned.back().lost = false;

  // The sync token is given to the ReleaseCallback.
  EXPECT_CALL(release, Released(returned[0].sync_token, false));
  provider().ReceiveReturnsFromParent(std::move(returned));
}

TEST_P(ClientResourceProviderTest, TransferableResourceSendTwoToParent) {
  TransferableResource tran[] = {MakeTransferableResource(use_gpu(), 'a', 15),
                                 MakeTransferableResource(use_gpu(), 'b', 16)};
  ResourceId id1 = provider().ImportResource(tran[0], base::DoNothing());
  ResourceId id2 = provider().ImportResource(tran[1], base::DoNothing());

  // Export the resource.
  std::vector<ResourceId> to_send = {id1, id2};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());
  ASSERT_EQ(exported.size(), 2u);

  // Exported resource matches except for the id which was mapped
  // to the local ResourceProvider, and the sync token should be
  // verified if it's a gpu resource.
  for (int i = 0; i < 2; ++i) {
    gpu::SyncToken verified_sync_token = tran[i].sync_token();
    if (!tran[i].is_software)
      verified_sync_token.SetVerifyFlush();
    EXPECT_EQ(exported[i].id, to_send[i]);
    EXPECT_EQ(exported[i].is_software, tran[i].is_software);
    EXPECT_EQ(exported[i].size, tran[i].size);
    EXPECT_EQ(exported[i].mailbox(), tran[i].mailbox());
    EXPECT_EQ(exported[i].sync_token(), verified_sync_token);
    EXPECT_EQ(exported[i].texture_target(), tran[i].texture_target());
  }

  provider().RemoveImportedResource(id1);
  provider().RemoveImportedResource(id2);
  DestroyProvider();
}

TEST_P(ClientResourceProviderTest, TransferableResourceSendToParentTwoTimes) {
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(tran, base::DoNothing());

  // Export the resource.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());
  ASSERT_EQ(exported.size(), 1u);
  EXPECT_EQ(exported[0].id, id);

  // Return the resource, with a sync token if using gpu.
  std::vector<ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = exported[0].id;
  if (use_gpu())
    returned.back().sync_token = SyncTokenFromUInt(31);
  returned.back().count = 1;
  returned.back().lost = false;
  provider().ReceiveReturnsFromParent(std::move(returned));

  // Then export again, it still sends.
  exported.clear();
  provider().PrepareSendToParent(to_send, &exported, context_provider());
  ASSERT_EQ(exported.size(), 1u);
  EXPECT_EQ(exported[0].id, id);

  provider().RemoveImportedResource(id);
  DestroyProvider();
}

TEST_P(ClientResourceProviderTest,
       TransferableResourceLostOnShutdownIfExported) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  provider().RemoveImportedResource(id);

  EXPECT_CALL(release, Released(_, true));
  DestroyProvider();
}

TEST_P(ClientResourceProviderTest, TransferableResourceSendToParentManyUnsent) {
  MockReleaseCallback release;

  // 5 resources to have 2 before and 2 after the one being sent and returned,
  // to verify the data structures are treated correctly when removing from the
  // middle. 1 after is not enough as that one will replace the resource being
  // removed and misses some edge cases. We want another resource after that in
  // the structure to verify it is also not treated incorrectly.
  struct Data {
    TransferableResource tran;
    ResourceId id;
  } data[5];
  for (int i = 0; i < 5; ++i) {
    data[i].tran = MakeTransferableResource(use_gpu(), 'a', 15);
    data[i].id = provider().ImportResource(
        data[i].tran, base::BindOnce(&MockReleaseCallback::Released,
                                     base::Unretained(&release)));
  }
  std::sort(std::begin(data), std::end(data),
            [](const Data& a, const Data& b) { return a.id < b.id; });

  // Export the resource.
  std::vector<ResourceId> to_send = {data[2].id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());
  ASSERT_EQ(exported.size(), 1u);

  // Exported resource matches except for the id which was mapped
  // to the local ResourceProvider, and the sync token should be
  // verified if it's a gpu resource.
  gpu::SyncToken verified_sync_token = data[2].tran.sync_token();
  if (!data[2].tran.is_software)
    verified_sync_token.SetVerifyFlush();

  // Exported resources are not released when removed, until the export returns.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(data[2].id);

  // Return the resource, with a sync token if using gpu.
  std::vector<ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = exported[0].id;
  if (use_gpu())
    returned.back().sync_token = SyncTokenFromUInt(31);
  returned.back().count = 1;
  returned.back().lost = false;

  // The sync token is given to the ReleaseCallback.
  EXPECT_CALL(release, Released(returned[0].sync_token, false));
  provider().ReceiveReturnsFromParent(std::move(returned));

  EXPECT_CALL(release, Released(_, false)).Times(4);
  provider().RemoveImportedResource(data[0].id);
  provider().RemoveImportedResource(data[1].id);
  provider().RemoveImportedResource(data[3].id);
  provider().RemoveImportedResource(data[4].id);
}

TEST_P(ClientResourceProviderTest, TransferableResourceRemovedAfterReturn) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  // Return the resource. This does not release the resource back to
  // the client.
  std::vector<ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = exported[0].id;
  if (use_gpu())
    returned.back().sync_token = SyncTokenFromUInt(31);
  returned.back().count = 1;
  returned.back().lost = false;

  EXPECT_CALL(release, Released(_, _)).Times(0);
  auto sync_token = returned.back().sync_token;
  provider().ReceiveReturnsFromParent(std::move(returned));
  testing::Mock::VerifyAndClearExpectations(&release);

  // Once removed, the resource is released.
  EXPECT_CALL(release, Released(sync_token, false));
  provider().RemoveImportedResource(id);
}

TEST_P(ClientResourceProviderTest, TransferableResourceExportedTwice) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource once.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  // Exported resources are not released when removed, until all exports are
  // returned.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Export the resource twice.
  exported = {};
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  {
    // Return the resource the first time.
    std::vector<ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = exported[0].id;
    if (use_gpu())
      returned.back().sync_token = SyncTokenFromUInt(31);
    returned.back().count = 1;
    returned.back().lost = false;
    provider().ReceiveReturnsFromParent(std::move(returned));
  }

  {
    // And a second time, with a different sync token. Now the ReleaseCallback
    // can happen, using the latest sync token.
    std::vector<ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = exported[0].id;
    if (use_gpu())
      returned.back().sync_token = SyncTokenFromUInt(47);
    returned.back().count = 1;
    returned.back().lost = false;
    EXPECT_CALL(release, Released(returned[0].sync_token, false));
    provider().ReceiveReturnsFromParent(std::move(returned));
  }
}

TEST_P(ClientResourceProviderTest, TransferableResourceReturnedTwiceAtOnce) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource once.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  // Exported resources are not released when removed, until all exports are
  // returned.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Export the resource twice.
  exported = {};
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  // Return both exports at once.
  std::vector<ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = exported[0].id;
  if (use_gpu())
    returned.back().sync_token = SyncTokenFromUInt(31);
  returned.back().count = 2;
  returned.back().lost = false;

  // When returned, the ReleaseCallback can happen, using the latest sync token.
  EXPECT_CALL(release, Released(returned[0].sync_token, false));
  provider().ReceiveReturnsFromParent(std::move(returned));
}

TEST_P(ClientResourceProviderTest, TransferableResourceLostOnReturn) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource once.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  // Exported resources are not released when removed, until all exports are
  // returned.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Export the resource twice.
  exported = {};
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  {
    // Return the resource the first time, not lost.
    std::vector<ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = exported[0].id;
    returned.back().count = 1;
    returned.back().lost = false;
    provider().ReceiveReturnsFromParent(std::move(returned));
  }

  {
    // Return a second time, as lost. The ReturnCallback should report it
    // lost.
    std::vector<ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = exported[0].id;
    returned.back().count = 1;
    returned.back().lost = true;
    EXPECT_CALL(release, Released(_, true));
    provider().ReceiveReturnsFromParent(std::move(returned));
  }
}

TEST_P(ClientResourceProviderTest, TransferableResourceLostOnFirstReturn) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId id = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Export the resource once.
  std::vector<ResourceId> to_send = {id};
  std::vector<TransferableResource> exported;
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  // Exported resources are not released when removed, until all exports are
  // returned.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Export the resource twice.
  exported = {};
  provider().PrepareSendToParent(to_send, &exported, context_provider());

  {
    // Return the resource the first time, marked as lost.
    std::vector<ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = exported[0].id;
    returned.back().count = 1;
    returned.back().lost = true;
    provider().ReceiveReturnsFromParent(std::move(returned));
  }

  {
    // Return a second time, not lost. The first lost signal should not be lost.
    std::vector<ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = exported[0].id;
    returned.back().count = 1;
    returned.back().lost = false;
    EXPECT_CALL(release, Released(_, true));
    provider().ReceiveReturnsFromParent(std::move(returned));
  }
}

TEST_P(ClientResourceProviderTest, ReturnedSyncTokensArePassedToClient) {
  // SyncTokens are gpu-only.
  if (!use_gpu())
    return;

  MockReleaseCallback release;

  auto* sii = context_provider()->SharedImageInterface();

  // NOTE: The parameters passed below are largely arbitrary, as the contents of
  // the SharedImage are not actually accessed in any way in this test.
  // Nonetheless, set SharedImage usage to model an SI that is written via
  // raster and then read by the display compositor (e.g., for canvas), in order
  // to match a use case that would actually be put into a TransferableResource
  // in production.
  gpu::Mailbox mailbox =
      sii->CreateSharedImage(
             {SinglePlaneFormat::kRGBA_8888, gfx::Size(1, 1), gfx::ColorSpace(),
              gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
                  gpu::SHARED_IMAGE_USAGE_DISPLAY_READ,
              "TestLabel"},
             gpu::kNullSurfaceHandle)
          ->mailbox();
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

  constexpr gfx::Size size(64, 64);
  auto tran = TransferableResource::MakeGpu(mailbox, GL_TEXTURE_2D, sync_token,
                                            size, SinglePlaneFormat::kRGBA_8888,
                                            false /* is_overlay_candidate */);
  ResourceId resource = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  EXPECT_TRUE(tran.sync_token().HasData());
  // All the logic below assumes that the sync token releases are all positive.
  EXPECT_LT(0u, tran.sync_token().release_count());

  // Transfer the resource, expect the sync points to be consistent.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resource}, &list, context_provider());
  ASSERT_EQ(1u, list.size());
  EXPECT_LE(sync_token.release_count(), list[0].sync_token().release_count());
  EXPECT_EQ(0,
            memcmp(mailbox.name, list[0].mailbox().name, sizeof(mailbox.name)));

  // Make a new texture id from the mailbox.
  context_provider()->RasterInterface()->WaitSyncTokenCHROMIUM(
      list[0].sync_token().GetConstData());
  context_provider()->RasterInterface()->GenSyncTokenCHROMIUM(
      list[0].mutable_sync_token().GetData());
  EXPECT_TRUE(list[0].sync_token().HasData());

  // Receive the resource, then delete it, expect the SyncTokens to be
  // consistent.
  provider().ReceiveReturnsFromParent(
      TransferableResource::ReturnResources(list));

  gpu::SyncToken returned_sync_token;
  EXPECT_CALL(release, Released(_, false))
      .WillOnce(testing::SaveArg<0>(&returned_sync_token));
  provider().RemoveImportedResource(resource);
  EXPECT_GE(returned_sync_token.release_count(),
            list[0].sync_token().release_count());
}

TEST_P(ClientResourceProviderTest, LostResourcesAreReturnedLost) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId resource = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Transfer the resource to the parent.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resource}, &list, context_provider());
  EXPECT_EQ(1u, list.size());

  // Receive it back marked lost.
  std::vector<ReturnedResource> returned_to_child;
  returned_to_child.push_back(list[0].ToReturnedResource());
  returned_to_child.back().lost = true;
  provider().ReceiveReturnsFromParent(std::move(returned_to_child));

  // Delete the resource in the child. Expect the resource to be lost.
  EXPECT_CALL(release, Released(_, true));
  provider().RemoveImportedResource(resource);
}

TEST_P(ClientResourceProviderTest, ShutdownLosesExportedResources) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId resource = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Transfer the resource to the parent.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resource}, &list, context_provider());
  EXPECT_EQ(1u, list.size());

  // Remove it in the ClientResourceProvider, but since it's exported it's not
  // returned yet.
  provider().RemoveImportedResource(resource);

  // Destroy the ClientResourceProvider, the resource is returned lost.
  EXPECT_CALL(release, Released(_, true));
  DestroyProvider();
}

TEST_P(ClientResourceProviderTest, ReleaseExportedResources) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId resource = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Transfer the resource to the parent.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resource}, &list, context_provider());
  EXPECT_EQ(1u, list.size());

  // Remove it in the ClientResourceProvider, but since it's exported it's not
  // returned yet.
  provider().RemoveImportedResource(resource);

  // Drop any exported resources. They are returned lost for gpu compositing,
  // since gpu resources are modified (in their metadata) while being used by
  // the parent.
  EXPECT_CALL(release, Released(_, use_gpu()));
  provider().ReleaseAllExportedResources(use_gpu());

  EXPECT_CALL(release, Released(_, _)).Times(0);
}

TEST_P(ClientResourceProviderTest, ReleaseExportedResourcesThenRemove) {
  MockReleaseCallback release;
  TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 15);
  ResourceId resource = provider().ImportResource(
      tran, base::BindOnce(&MockReleaseCallback::Released,
                           base::Unretained(&release)));

  // Transfer the resource to the parent.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resource}, &list, context_provider());
  EXPECT_EQ(1u, list.size());

  // Drop any exported resources. They are now considered lost for gpu
  // compositing, since gpu resources are modified (in their metadata) while
  // being used by the parent.
  provider().ReleaseAllExportedResources(use_gpu());

  EXPECT_CALL(release, Released(_, use_gpu()));
  // Remove it in the ClientResourceProvider, it was exported so wouldn't be
  // released here, except that we dropped the export above.
  provider().RemoveImportedResource(resource);

  EXPECT_CALL(release, Released(_, _)).Times(0);
}

TEST_P(ClientResourceProviderTest, ReleaseMultipleResources) {
  MockReleaseCallback release;

  // Make 5 resources, put them in a non-sorted order.
  ResourceId resources[5];
  for (int i = 0; i < 5; ++i) {
    TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 1 + i);
    resources[i] = provider().ImportResource(
        tran, base::BindOnce(&MockReleaseCallback::ReleasedWithId,
                             base::Unretained(&release), ResourceId(i)));
  }

  // Transfer some resources to the parent, but not in the sorted order.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resources[2], resources[0], resources[4]},
                                 &list, context_provider());
  EXPECT_EQ(3u, list.size());

  // Receive them back. Since these are not in the same order they were
  // inserted originally, we verify the ClientResourceProvider can handle
  // resources being returned (in a group) that are not at the front/back
  // of its internal storage. IOW This shouldn't crash or corrupt state.
  std::vector<ReturnedResource> returned_to_child;
  returned_to_child.push_back(list[0].ToReturnedResource());
  returned_to_child.push_back(list[1].ToReturnedResource());
  returned_to_child.push_back(list[2].ToReturnedResource());
  provider().ReceiveReturnsFromParent(std::move(returned_to_child));

  // Remove them from the ClientResourceProvider, they should be returned as
  // they're no longer exported.
  EXPECT_CALL(release, ReleasedWithId(kInvalidResourceId, _, false));
  provider().RemoveImportedResource(resources[0]);
  EXPECT_CALL(release, ReleasedWithId(ResourceId(2), _, false));
  provider().RemoveImportedResource(resources[2]);
  EXPECT_CALL(release, ReleasedWithId(ResourceId(4), _, false));
  provider().RemoveImportedResource(resources[4]);

  // These were never exported.
  EXPECT_CALL(release, ReleasedWithId(ResourceId(1), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(3), _, false));
  provider().RemoveImportedResource(resources[1]);
  provider().RemoveImportedResource(resources[3]);

  EXPECT_CALL(release, Released(_, false)).Times(0);
}

TEST_P(ClientResourceProviderTest, ReleaseMultipleResourcesBeforeReturn) {
  MockReleaseCallback release;

  // Make 5 resources, put them in a non-sorted order.
  ResourceId resources[5];
  for (int i = 0; i < 5; ++i) {
    TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 1 + i);
    resources[i] = provider().ImportResource(
        tran, base::BindOnce(&MockReleaseCallback::ReleasedWithId,
                             base::Unretained(&release), ResourceId(i)));
  }

  // Transfer some resources to the parent, but not in the sorted order.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resources[2], resources[0], resources[4]},
                                 &list, context_provider());
  EXPECT_EQ(3u, list.size());

  // Remove the exported resources from the ClientResourceProvider, they should
  // not yet be returned, since they are exported.
  provider().RemoveImportedResource(resources[0]);
  provider().RemoveImportedResource(resources[2]);
  provider().RemoveImportedResource(resources[4]);

  // Receive them back now. Since these are not in the same order they were
  // inserted originally, we verify the ClientResourceProvider can handle
  // resources being returned (in a group) that are not at the front/back
  // of its internal storage. IOW This shouldn't crash or corrupt state.
  std::vector<ReturnedResource> returned_to_child;
  returned_to_child.push_back(list[0].ToReturnedResource());
  returned_to_child.push_back(list[1].ToReturnedResource());
  returned_to_child.push_back(list[2].ToReturnedResource());
  // The resources are returned immediately since they were previously removed.
  EXPECT_CALL(release, ReleasedWithId(kInvalidResourceId, _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(2), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(4), _, false));
  provider().ReceiveReturnsFromParent(std::move(returned_to_child));

  // These were never exported.
  EXPECT_CALL(release, ReleasedWithId(ResourceId(1), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(3), _, false));
  provider().RemoveImportedResource(resources[1]);
  provider().RemoveImportedResource(resources[3]);

  EXPECT_CALL(release, Released(_, false)).Times(0);
}

TEST_P(ClientResourceProviderTest, ReturnDuplicateResourceBeforeRemove) {
  MockReleaseCallback release;

  // Make 5 resources, put them in a non-sorted order.
  ResourceId resources[5];
  for (int i = 0; i < 5; ++i) {
    TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 1 + i);
    resources[i] = provider().ImportResource(
        tran, base::BindOnce(&MockReleaseCallback::ReleasedWithId,
                             base::Unretained(&release), ResourceId(i)));
  }

  // Transfer a resource to the parent, do it twice.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resources[2]}, &list, context_provider());
  list.clear();
  provider().PrepareSendToParent({resources[2]}, &list, context_provider());

  // Receive the resource back. It's possible that the parent may return
  // the same ResourceId multiple times in the same message, which we test
  // for here.
  std::vector<ReturnedResource> returned_to_child;
  returned_to_child.push_back(list[0].ToReturnedResource());
  returned_to_child.push_back(list[0].ToReturnedResource());
  provider().ReceiveReturnsFromParent(std::move(returned_to_child));

  // Remove it from the ClientResourceProvider, it should be returned as
  // it's no longer exported.
  EXPECT_CALL(release, ReleasedWithId(ResourceId(2), _, false));
  provider().RemoveImportedResource(resources[2]);

  // These were never exported.
  EXPECT_CALL(release, ReleasedWithId(kInvalidResourceId, _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(1), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(3), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(4), _, false));
  provider().RemoveImportedResource(resources[0]);
  provider().RemoveImportedResource(resources[1]);
  provider().RemoveImportedResource(resources[3]);
  provider().RemoveImportedResource(resources[4]);

  EXPECT_CALL(release, Released(_, false)).Times(0);
}

TEST_P(ClientResourceProviderTest, ReturnDuplicateResourceAfterRemove) {
  MockReleaseCallback release;

  // Make 5 resources, put them in a non-sorted order.
  ResourceId resources[5];
  for (int i = 0; i < 5; ++i) {
    TransferableResource tran = MakeTransferableResource(use_gpu(), 'a', 1 + i);
    resources[i] = provider().ImportResource(
        tran, base::BindOnce(&MockReleaseCallback::ReleasedWithId,
                             base::Unretained(&release), ResourceId(i)));
  }

  // Transfer a resource to the parent, do it twice.
  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({resources[2]}, &list, context_provider());
  list.clear();
  provider().PrepareSendToParent({resources[2]}, &list, context_provider());

  // Remove it from the ClientResourceProvider, it should not be returned yet
  // as it's still exported.
  provider().RemoveImportedResource(resources[2]);

  // Receive the resource back. It's possible that the parent may return
  // the same ResourceId multiple times in the same message, which we test
  // for here.
  std::vector<ReturnedResource> returned_to_child;
  returned_to_child.push_back(list[0].ToReturnedResource());
  returned_to_child.push_back(list[0].ToReturnedResource());
  // Once no longer exported, since it was removed earlier, it will be returned
  // immediately.
  EXPECT_CALL(release, ReleasedWithId(ResourceId(2), _, false));
  provider().ReceiveReturnsFromParent(std::move(returned_to_child));

  // These were never exported.
  EXPECT_CALL(release, ReleasedWithId(kInvalidResourceId, _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(1), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(3), _, false));
  EXPECT_CALL(release, ReleasedWithId(ResourceId(4), _, false));
  provider().RemoveImportedResource(resources[0]);
  provider().RemoveImportedResource(resources[1]);
  provider().RemoveImportedResource(resources[3]);
  provider().RemoveImportedResource(resources[4]);

  EXPECT_CALL(release, Released(_, false)).Times(0);
}

// Tests that resources which are locked, are released after eviction has
// occurred, once they are also returned.
TEST_P(ClientResourceProviderTest, EvictionUnlocksResources) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kEvictionUnlocksResources},
                                       {});
  // Mark visible so eviction path is not inadvertently triggered.
  provider().SetVisible(true);

  MockReleaseCallback release;
  const uint32_t sync_token_value = 1u;
  TransferableResource resource =
      MakeTransferableResource(use_gpu(), 'a', sync_token_value);
  ResourceId id =
      provider().ImportResource(resource,
                                base::BindOnce(&MockReleaseCallback::Released,
                                               base::Unretained(&release)),
                                ReleaseCallback(),
                                base::BindOnce(&MockReleaseCallback::Evicted,
                                               base::Unretained(&release)));

  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({id}, &list, context_provider());
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].id, id);

  // Eviction alone should not delete the resource, nor have called the Eviction
  // callback.
  EXPECT_CALL(release, Evicted).Times(0);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetEvicted(true);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Becoming hidden and evicted should trigger the callback. The resource
  // should not be deleted yet, until it has bene returned.
  EXPECT_CALL(release, Evicted).Times(1);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetVisible(false);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Clients are expected to call RemoveImportedResource in response to Evicted.
  // However being exported, should not trigger a Released callback.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Returning of resources should release the previously locked resource.
  std::vector<ReturnedResource> returned;
  returned.push_back(list[0].ToReturnedResource());
  EXPECT_CALL(release, Released(_, false));
  provider().ReceiveReturnsFromParent(std::move(returned));
  EXPECT_EQ(provider().num_resources_for_testing(), 0u);
}

// Tests that when resources are returned in the middle of an eviction process,
// that we release any locked resources once the eviction has completed.
TEST_P(ClientResourceProviderTest,
       LockedReturnedResourcesReleasedDuringEviction) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kEvictionUnlocksResources},
                                       {});
  // Mark visible so eviction path is not inadvertently triggered.
  provider().SetVisible(true);

  MockReleaseCallback release;
  const uint32_t sync_token_value = 1u;
  TransferableResource resource =
      MakeTransferableResource(use_gpu(), 'a', sync_token_value);
  ResourceId id =
      provider().ImportResource(resource,
                                base::BindOnce(&MockReleaseCallback::Released,
                                               base::Unretained(&release)),
                                ReleaseCallback(),
                                base::BindOnce(&MockReleaseCallback::Evicted,
                                               base::Unretained(&release)));

  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({id}, &list, context_provider());
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].id, id);

  // Eviction alone should not delete the resource, nor have called the Eviction
  // callback.
  EXPECT_CALL(release, Evicted).Times(0);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetEvicted(true);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Returning of resources will not return them while we are still visible.
  std::vector<ReturnedResource> returned;
  returned.push_back(list[0].ToReturnedResource());
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().ReceiveReturnsFromParent(std::move(returned));
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Becoming hidden and evicted should trigger the callback. The resources
  // having previously been returned, should now be released.
  EXPECT_CALL(release, Evicted).Times(1);
  provider().SetVisible(false);

  // Clients are expected to call RemoveImportedResource in response to Evicted.
  // Being no longer exported, this should call Released.
  EXPECT_CALL(release, Released(_, false));
  provider().RemoveImportedResource(id);
  EXPECT_EQ(provider().num_resources_for_testing(), 0u);
}

// Tests that when a Client has removed a resource, which has not yet been
// returned, that we do not notify the Client of eviction.
TEST_P(ClientResourceProviderTest, RemovedEvictedResourcesDoNotNotifyClient) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kEvictionUnlocksResources},
                                       {});
  // Mark visible so eviction path is not inadvertently triggered.
  provider().SetVisible(true);

  MockReleaseCallback release;
  const uint32_t sync_token_value = 1u;
  TransferableResource resource =
      MakeTransferableResource(use_gpu(), 'a', sync_token_value);
  ResourceId id =
      provider().ImportResource(resource,
                                base::BindOnce(&MockReleaseCallback::Released,
                                               base::Unretained(&release)),
                                ReleaseCallback(),
                                base::BindOnce(&MockReleaseCallback::Evicted,
                                               base::Unretained(&release)));

  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({id}, &list, context_provider());
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].id, id);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // After removing the resource should still exist, as it has yet to be
  // returned.
  provider().RemoveImportedResource(id);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Eviction alone should not delete the resource, nor have called the Eviction
  // callback.
  EXPECT_CALL(release, Evicted).Times(0);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetEvicted(true);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Becoming hidden and evicted would normally call `Evicted` however it
  // should not, since the Client removed it earlier. The resource should not be
  // deleted yet, until it has bene returned.
  EXPECT_CALL(release, Evicted).Times(0);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetVisible(false);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Returning of resources should release the previously locked resource.
  std::vector<ReturnedResource> returned;
  returned.push_back(list[0].ToReturnedResource());
  EXPECT_CALL(release, Released(_, false));
  provider().ReceiveReturnsFromParent(std::move(returned));
  EXPECT_EQ(provider().num_resources_for_testing(), 0u);
}

// Tests that when we provide Main thread callbacks, that they are ran after
// the resources have returned. Also confirming that we Flush resources once
// callbacks are processed.
TEST_P(ClientResourceProviderTest, EvictionNotifiesMainAndFlushes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kEvictionUnlocksResources,
       features::kBatchMainThreadReleaseCallbacks},
      {});
  // Recreate to support `features::kBatchMainThreadReleaseCallbacks`.
  InitProvider();
  // Mark visible so eviction path is not inadvertently triggered.
  provider().SetVisible(true);

  MockReleaseCallback release;
  const uint32_t sync_token_value = 1u;
  TransferableResource resource =
      MakeTransferableResource(use_gpu(), 'a', sync_token_value);
  ResourceId id =
      provider().ImportResource(resource, ReleaseCallback(),
                                base::BindOnce(&MockReleaseCallback::Released,
                                               base::Unretained(&release)),
                                base::BindOnce(&MockReleaseCallback::Evicted,
                                               base::Unretained(&release)));

  std::vector<TransferableResource> list;
  provider().PrepareSendToParent({id}, &list, context_provider());
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].id, id);

  // Eviction alone should not delete the resource, nor have called the Eviction
  // callback.
  EXPECT_CALL(release, Evicted).Times(0);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetEvicted(true);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Becoming hidden and evicted should trigger the callback. The resource
  // should not be deleted yet, until it has bene returned.
  EXPECT_CALL(release, Evicted).Times(1);
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().SetVisible(false);
  EXPECT_EQ(provider().num_resources_for_testing(), 1u);

  // Clients are expected to call RemoveImportedResource in response to Evicted.
  // However being exported, should not trigger a Released callback.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Returning of resources will enqueue main callbacks. The internal resource
  // should have been deleted.
  std::vector<ReturnedResource> returned;
  returned.push_back(list[0].ToReturnedResource());
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().ReceiveReturnsFromParent(std::move(returned));
  EXPECT_EQ(provider().num_resources_for_testing(), 0u);

  // The enqueued Released callback should be invoked, along with the Flush.
  EXPECT_CALL(release, Released(_, false));
  ExpectFlush();
  VizTestSuite::RunUntilIdle();
}

// Tests that when we are using
// `ClientResourceProvider::ScopedBatchResourcesRelease` that callbacks are not
// immediately ran when we remove resources. Confirming that they are ran once
// the scope is exited.
TEST_P(ClientResourceProviderTest, BatchedCallbacksDoNotFireImmediately) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kBatchResourceRelease}, {});
  // Mark visible so eviction path is not inadvertently triggered.
  provider().SetVisible(true);

  // We only import the resource and do not `PrepareSendToParent`. As `exported`
  // resources are not removed by `RemoveImportedResource`.
  MockReleaseCallback release;
  const uint32_t sync_token_value = 1u;
  TransferableResource resource =
      MakeTransferableResource(use_gpu(), 'a', sync_token_value);
  ResourceId id =
      provider().ImportResource(resource, ReleaseCallback(),
                                base::BindOnce(&MockReleaseCallback::Released,
                                               base::Unretained(&release)),
                                base::BindOnce(&MockReleaseCallback::Evicted,
                                               base::Unretained(&release)));

  {
    // We use `ScopedBatchResourcesRelease` to prevent the immediate callbacks.
    ClientResourceProvider::ScopedBatchResourcesRelease batch =
        provider().CreateScopedBatchResourcesRelease();
    // Zero callbacks for the removal, they should run when `batch` leaves
    // scoped.
    EXPECT_CALL(release, Released(_, _)).Times(0);
    provider().RemoveImportedResource(id);
    // Leaving scope should lead to `batch` triggering the callback
    EXPECT_CALL(release, Released(_, _));
  }

  ExpectFlush();
  VizTestSuite::RunUntilIdle();
}

// Ensures that while batching callbacks, that `ClientResourceProvider` being
// destroyed before the `ClientResourceProvider::ScopedBatchResourcesRelease`
// ensures the callbacks are ran.
TEST_P(ClientResourceProviderTest,
       BatchedCallbacksFireUponProviderDestruction) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kBatchResourceRelease}, {});
  // Mark visible so eviction path is not inadvertently triggered.
  provider().SetVisible(true);

  // We only import the resource and do not `PrepareSendToParent`. As `exported`
  // resources are not removed by `RemoveImportedResource`.
  MockReleaseCallback release;
  const uint32_t sync_token_value = 1u;
  TransferableResource resource =
      MakeTransferableResource(use_gpu(), 'a', sync_token_value);
  ResourceId id =
      provider().ImportResource(resource, ReleaseCallback(),
                                base::BindOnce(&MockReleaseCallback::Released,
                                               base::Unretained(&release)),
                                base::BindOnce(&MockReleaseCallback::Evicted,
                                               base::Unretained(&release)));

  // We use `ScopedBatchResourcesRelease` to prevent the immediate callbacks.
  ClientResourceProvider::ScopedBatchResourcesRelease batch =
      provider().CreateScopedBatchResourcesRelease();
  // Zero callbacks for the removal, they should run when `batch` leaves
  // scoped.
  EXPECT_CALL(release, Released(_, _)).Times(0);
  provider().RemoveImportedResource(id);

  // Destroying `provider` should run the callback.
  EXPECT_CALL(release, Released(_, _));
  DestroyProvider();
  ExpectFlush();
  VizTestSuite::RunUntilIdle();
}

}  // namespace
}  // namespace viz

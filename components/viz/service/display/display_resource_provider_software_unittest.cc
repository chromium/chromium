// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_software.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "build/build_config.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  MOCK_METHOD3(Released,
               void(scoped_refptr<gpu::ClientSharedImage> shared_image,
                    const gpu::SyncToken& token,
                    bool lost));
};

static void CollectResources(std::vector<ReturnedResource>* array,
                             std::vector<ReturnedResource> returned) {
  array->insert(array->end(), std::make_move_iterator(returned.begin()),
                std::make_move_iterator(returned.end()));
}

class DisplayResourceProviderSoftwareTest : public testing::Test {
 public:
  DisplayResourceProviderSoftwareTest() = default;

  ~DisplayResourceProviderSoftwareTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  void InitializeProvider() {
    auto context_provider = base::MakeRefCounted<TestInProcessContextProvider>(
        TestContextType::kSoftwareRaster, /*support_locking=*/false);
    gpu::ContextResult result = context_provider->BindToCurrentSequence();
    CHECK_EQ(result, gpu::ContextResult::kSuccess);
    auto* gpu_service = context_provider->GpuService();
    child_context_provider_ = std::move(context_provider);

    resource_provider_ = std::make_unique<DisplayResourceProviderSoftware>(
        /*shared_bitmap_manager=*/nullptr, gpu_service->shared_image_manager(),
        gpu_service->sync_point_manager(), gpu_service->gpu_scheduler());

    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
  }

  ResourceId AllocateAndFillSoftwareResource(
      MockReleaseCallback& release_callback,
      const gfx::Size& size,
      const uint32_t value) {
    auto* shared_image_interface =
        child_context_provider_->SharedImageInterface();
    auto shared_image_mapping = shared_image_interface->CreateSharedImage(
        {SinglePlaneFormat::kBGRA_8888, size, gfx::ColorSpace(),
         gpu::SHARED_IMAGE_USAGE_CPU_WRITE,
         "DisplayResourceProviderSoftwareTest"});

    base::span<uint32_t> span =
        shared_image_mapping.mapping.GetMemoryAsSpan<uint32_t>(size.GetArea());
    std::fill(span.begin(), span.end(), value);

    auto transferable_resource = TransferableResource::MakeSoftwareSharedImage(
        shared_image_mapping.shared_image,
        shared_image_interface->GenVerifiedSyncToken(), size,
        SinglePlaneFormat::kBGRA_8888,
        TransferableResource::ResourceSource::kTileRasterTask);

    auto callback = base::BindOnce(
        &MockReleaseCallback::Released, base::Unretained(&release_callback),
        std::move(shared_image_mapping.shared_image));

    return child_resource_provider_->ImportResource(
        std::move(transferable_resource), std::move(callback));
  }

 protected:
  scoped_refptr<RasterContextProvider> child_context_provider_;
  std::unique_ptr<DisplayResourceProviderSoftware> resource_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
};

TEST_F(DisplayResourceProviderSoftwareTest, ReadSoftwareResources) {
  InitializeProvider();

  gfx::Size size(64, 64);
  const uint32_t kBadBeef = 0xbadbeef;

  MockReleaseCallback release_callback;
  ResourceId resource_id =
      AllocateAndFillSoftwareResource(release_callback, size, kBadBeef);

  // Transfer resources to the parent.
  std::vector<TransferableResource> send_to_parent;
  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child), SurfaceId());
  child_resource_provider_->PrepareSendToParent(
      {resource_id}, &send_to_parent,
      static_cast<RasterContextProvider*>(nullptr));
  resource_provider_->ReceiveFromChild(child_id, send_to_parent);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceId mapped_resource_id = resource_map[resource_id];
  {
    SkBitmap dstBitmap;
    dstBitmap.allocPixels(SkImageInfo::Make(size.width(), size.height(),
                                            kBGRA_8888_SkColorType,
                                            kPremul_SkAlphaType));

    DisplayResourceProviderSoftware::ScopedReadLockSkImage lock(
        resource_provider_.get(), mapped_resource_id, kPremul_SkAlphaType);
    const SkImage* sk_image = lock.sk_image();
    bool result = sk_image->readPixels(nullptr, dstBitmap.pixmap(),
                                       /*srcX=*/0, /*srcY=*/0);

    EXPECT_TRUE(result);
    EXPECT_EQ(sk_image->width(), size.width());
    EXPECT_EQ(sk_image->height(), size.height());
    EXPECT_EQ(*dstBitmap.getAddr32(16, 16), kBadBeef);
  }

  EXPECT_EQ(0u, returned_to_child.size());
  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));

  EXPECT_CALL(release_callback, Released(_, _, false));
  child_resource_provider_->RemoveImportedResource(resource_id);
}

}  // namespace
}  // namespace viz

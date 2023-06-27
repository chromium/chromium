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
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
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
  MOCK_METHOD2(Released, void(const gpu::SyncToken& token, bool lost));
};

static void CollectResources(std::vector<ReturnedResource>* array,
                             std::vector<ReturnedResource> returned) {
  array->insert(array->end(), std::make_move_iterator(returned.begin()),
                std::make_move_iterator(returned.end()));
}

static SharedBitmapId CreateAndFillSharedBitmap(SharedBitmapManager* manager,
                                                const gfx::Size& size,
                                                SharedImageFormat format,
                                                uint32_t value) {
  SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();

  base::MappedReadOnlyRegion shm =
      bitmap_allocation::AllocateSharedBitmap(size, format);
  manager->ChildAllocatedSharedBitmap(shm.region.Map(), shared_bitmap_id);
  base::span<uint32_t> span =
      shm.mapping.GetMemoryAsSpan<uint32_t>(size.GetArea());
  std::fill(span.begin(), span.end(), value);
  return shared_bitmap_id;
}

class DisplayResourceProviderSoftwareTest : public testing::Test {
 public:
  DisplayResourceProviderSoftwareTest()
      : shared_bitmap_manager_(std::make_unique<TestSharedBitmapManager>()),
        resource_provider_(std::make_unique<DisplayResourceProviderSoftware>(
            shared_bitmap_manager_.get())),
        child_resource_provider_(std::make_unique<ClientResourceProvider>()) {}

  ~DisplayResourceProviderSoftwareTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

 protected:
  const std::unique_ptr<TestSharedBitmapManager> shared_bitmap_manager_;
  const std::unique_ptr<DisplayResourceProviderSoftware> resource_provider_;
  const std::unique_ptr<ClientResourceProvider> child_resource_provider_;
};

TEST_F(DisplayResourceProviderSoftwareTest, ReadSoftwareResources) {
  gfx::Size size(64, 64);
  SharedImageFormat format = SinglePlaneFormat::kRGBA_8888;
  const uint32_t kBadBeef = 0xbadbeef;
  SharedBitmapId shared_bitmap_id = CreateAndFillSharedBitmap(
      shared_bitmap_manager_.get(), size, format, kBadBeef);

  auto resource =
      TransferableResource::MakeSoftware(shared_bitmap_id, size, format);

  MockReleaseCallback release;
  ResourceId resource_id = child_resource_provider_->ImportResource(
      resource, base::BindOnce(&MockReleaseCallback::Released,
                               base::Unretained(&release)));
  EXPECT_NE(kInvalidResourceId, resource_id);

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
    DisplayResourceProviderSoftware::ScopedReadLockSkImage lock(
        resource_provider_.get(), mapped_resource_id, kPremul_SkAlphaType);
    const SkImage* sk_image = lock.sk_image();
    SkBitmap sk_bitmap;
    sk_image->asLegacyBitmap(&sk_bitmap);
    EXPECT_EQ(sk_image->width(), size.width());
    EXPECT_EQ(sk_image->height(), size.height());
    EXPECT_EQ(*sk_bitmap.getAddr32(16, 16), kBadBeef);
  }

  EXPECT_EQ(0u, returned_to_child.size());
  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(
      std::move(returned_to_child));

  EXPECT_CALL(release, Released(_, false));
  child_resource_provider_->RemoveImportedResource(resource_id);
}

}  // namespace
}  // namespace viz

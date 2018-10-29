// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/client/client_resource_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "components/viz/test/test_texture.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace viz {
namespace {

class MockReleaseCallback {
 public:
  MOCK_METHOD2(Released, void(const gpu::SyncToken& token, bool lost));
};

MATCHER_P(MatchesSyncToken, sync_token, "") {
  gpu::SyncToken other;
  memcpy(&other, arg, sizeof(other));
  return other == sync_token;
}

static void CollectResources(std::vector<ReturnedResource>* array,
                             const std::vector<ReturnedResource>& returned) {
  array->insert(array->end(), returned.begin(), returned.end());
}

static SharedBitmapId CreateAndFillSharedBitmap(SharedBitmapManager* manager,
                                                const gfx::Size& size,
                                                ResourceFormat format,
                                                uint32_t value) {
  SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();

  std::unique_ptr<base::SharedMemory> shm =
      bitmap_allocation::AllocateMappedBitmap(size, RGBA_8888);
  manager->ChildAllocatedSharedBitmap(
      bitmap_allocation::DuplicateAndCloseMappedBitmap(shm.get(), size,
                                                       RGBA_8888),
      shared_bitmap_id);

  std::fill_n(static_cast<uint32_t*>(shm->memory()), size.GetArea(), value);
  return shared_bitmap_id;
}

// Shared data between multiple ResourceProviderGLES2Interface. This contains
// mailbox contents as well as information about sync points.
class ContextSharedData {
 public:
  static std::unique_ptr<ContextSharedData> Create() {
    return base::WrapUnique(new ContextSharedData());
  }

  uint32_t InsertFenceSync() { return next_fence_sync_++; }

  void GenMailbox(GLbyte* mailbox) {
    memset(mailbox, 0, GL_MAILBOX_SIZE_CHROMIUM);
    memcpy(mailbox, &next_mailbox_, sizeof(next_mailbox_));
    ++next_mailbox_;
  }

  void ProduceTexture(const GLbyte* mailbox_name,
                      const gpu::SyncToken& sync_token,
                      scoped_refptr<TestTexture> texture) {
    uint32_t sync_point = static_cast<uint32_t>(sync_token.release_count());

    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    ASSERT_TRUE(mailbox && mailbox < next_mailbox_);
    textures_[mailbox] = texture;
    ASSERT_LT(sync_point_for_mailbox_[mailbox], sync_point);
    sync_point_for_mailbox_[mailbox] = sync_point;
  }

  scoped_refptr<TestTexture> ConsumeTexture(const GLbyte* mailbox_name,
                                            const gpu::SyncToken& sync_token) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    DCHECK(mailbox && mailbox < next_mailbox_);

    // If the latest sync point the context has waited on is before the sync
    // point for when the mailbox was set, pretend we never saw that
    // ProduceTexture.
    if (sync_point_for_mailbox_[mailbox] > sync_token.release_count()) {
      NOTREACHED();
      return scoped_refptr<TestTexture>();
    }
    return textures_[mailbox];
  }

 private:
  ContextSharedData() : next_fence_sync_(1), next_mailbox_(1) {}

  uint64_t next_fence_sync_;
  unsigned next_mailbox_;
  using TextureMap = std::unordered_map<unsigned, scoped_refptr<TestTexture>>;
  TextureMap textures_;
  std::unordered_map<unsigned, uint32_t> sync_point_for_mailbox_;
};

class ResourceProviderGLES2Interface : public TestGLES2Interface {
 public:
  explicit ResourceProviderGLES2Interface(ContextSharedData* shared_data)
      : shared_data_(shared_data) {}

  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override {
    uint64_t fence_sync = shared_data_->InsertFenceSync();
    gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                                   fence_sync);
    sync_token_data.SetVerifyFlush();
    // Commit the ProduceTextureDirectCHROMIUM calls at this point, so that
    // they're associated with the sync point.
    for (const std::unique_ptr<PendingProduceTexture>& pending_texture :
         pending_produce_textures_) {
      shared_data_->ProduceTexture(pending_texture->mailbox, sync_token_data,
                                   pending_texture->texture);
    }
    pending_produce_textures_.clear();
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    if (sync_token)
      memcpy(&sync_token_data, sync_token, sizeof(sync_token_data));

    if (sync_token_data.release_count() >
        last_waited_sync_token_.release_count()) {
      last_waited_sync_token_ = sync_token_data;
    }
  }

  const gpu::SyncToken& last_waited_sync_token() const {
    return last_waited_sync_token_;
  }

  void TexStorage2DEXT(GLenum target,
                       GLint levels,
                       GLuint internalformat,
                       GLint width,
                       GLint height) override {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_EQ(1, levels);
    GLenum format = GL_RGBA;
    switch (internalformat) {
      case GL_RGBA8_OES:
        break;
      case GL_BGRA8_EXT:
        format = GL_BGRA_EXT;
        break;
      default:
        NOTREACHED();
    }
    AllocateTexture(gfx::Size(width, height), format);
  }

  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(internalformat, static_cast<GLint>(format));
    ASSERT_FALSE(border);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    AllocateTexture(gfx::Size(width, height), format);
    if (pixels)
      SetPixels(0, 0, width, height, pixels);
  }

  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    {
      base::AutoLock lock_for_texture_access(namespace_->lock);
      ASSERT_EQ(GLDataFormat(BoundTexture(target)->format), format);
    }
    ASSERT_TRUE(pixels);
    SetPixels(xoffset, yoffset, width, height, pixels);
  }

  void ProduceTextureDirectCHROMIUM(GLuint texture, GLbyte* mailbox) override {
    shared_data_->GenMailbox(mailbox);
    // Delay moving the texture into the mailbox until the next
    // sync token, so that it is not visible to other contexts that
    // haven't waited on that sync point.
    std::unique_ptr<PendingProduceTexture> pending(new PendingProduceTexture);
    memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
    base::AutoLock lock_for_texture_access(namespace_->lock);
    pending->texture = UnboundTexture(texture);
    pending_produce_textures_.push_back(std::move(pending));
  }

  GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override {
    GLuint texture_id;
    GenTextures(1, &texture_id);

    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<TestTexture> texture =
        shared_data_->ConsumeTexture(mailbox, last_waited_sync_token_);
    namespace_->textures.Replace(texture_id, texture);
    return texture_id;
  }

  void GetPixels(const gfx::Size& size,
                 ResourceFormat format,
                 uint8_t* pixels) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<TestTexture> texture = BoundTexture(GL_TEXTURE_2D);
    ASSERT_EQ(texture->size, size);
    ASSERT_EQ(texture->format, format);
    memcpy(pixels, texture->data.get(), TextureSizeBytes(size, format));
  }

 private:
  void AllocateTexture(const gfx::Size& size, GLenum format) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    ResourceFormat texture_format = RGBA_8888;
    switch (format) {
      case GL_RGBA:
        texture_format = RGBA_8888;
        break;
      case GL_BGRA_EXT:
        texture_format = BGRA_8888;
        break;
    }
    base::AutoLock lock_for_texture_access(namespace_->lock);
    BoundTexture(GL_TEXTURE_2D)->Reallocate(size, texture_format);
  }

  void SetPixels(int xoffset,
                 int yoffset,
                 int width,
                 int height,
                 const void* pixels) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<TestTexture> texture = BoundTexture(GL_TEXTURE_2D);
    ASSERT_TRUE(texture->data.get());
    ASSERT_TRUE(xoffset >= 0 && xoffset + width <= texture->size.width());
    ASSERT_TRUE(yoffset >= 0 && yoffset + height <= texture->size.height());
    ASSERT_TRUE(pixels);
    size_t in_pitch = TextureSizeBytes(gfx::Size(width, 1), texture->format);
    size_t out_pitch =
        TextureSizeBytes(gfx::Size(texture->size.width(), 1), texture->format);
    uint8_t* dest = texture->data.get() + yoffset * out_pitch +
                    TextureSizeBytes(gfx::Size(xoffset, 1), texture->format);
    const uint8_t* src = static_cast<const uint8_t*>(pixels);
    for (int i = 0; i < height; ++i) {
      memcpy(dest, src, in_pitch);
      dest += out_pitch;
      src += in_pitch;
    }
  }

  struct PendingProduceTexture {
    GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
    scoped_refptr<TestTexture> texture;
  };
  ContextSharedData* shared_data_;
  gpu::SyncToken last_waited_sync_token_;
  std::vector<std::unique_ptr<PendingProduceTexture>> pending_produce_textures_;
};

class DisplayResourceProviderTest : public testing::TestWithParam<bool> {
 public:
  explicit DisplayResourceProviderTest(bool child_needs_sync_token)
      : use_gpu_(GetParam()),
        child_needs_sync_token_(child_needs_sync_token),
        shared_data_(ContextSharedData::Create()) {
    if (use_gpu_) {
      auto gl_owned =
          std::make_unique<ResourceProviderGLES2Interface>(shared_data_.get());
      gl_ = gl_owned.get();
      context_provider_ = TestContextProvider::Create(std::move(gl_owned));
      context_provider_->UnboundTestContextGL()
          ->set_support_texture_format_bgra8888(true);
      context_provider_->BindToCurrentThread();

      auto child_gl_owned =
          std::make_unique<ResourceProviderGLES2Interface>(shared_data_.get());
      child_gl_ = child_gl_owned.get();
      child_context_provider_ =
          TestContextProvider::Create(std::move(child_gl_owned));
      child_context_provider_->UnboundTestContextGL()
          ->set_support_texture_format_bgra8888(true);
      child_context_provider_->BindToCurrentThread();
      gpu_memory_buffer_manager_ =
          std::make_unique<TestGpuMemoryBufferManager>();
    }
    // SharedBitmapManager may always be present, even if gpu compositing.
    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();

    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        use_gpu_ ? DisplayResourceProvider::kGpu
                 : DisplayResourceProvider::kSoftware,
        context_provider_.get(), shared_bitmap_manager_.get());

    MakeChildResourceProvider();
  }

  DisplayResourceProviderTest() : DisplayResourceProviderTest(true) {}

  ~DisplayResourceProviderTest() {
    if (child_resource_provider_)
      child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  bool use_gpu() const { return use_gpu_; }

  void MakeChildResourceProvider() {
    child_resource_provider_ =
        std::make_unique<ClientResourceProvider>(child_needs_sync_token_);
  }

  static ReturnCallback GetReturnCallback(
      std::vector<ReturnedResource>* array) {
    return base::BindRepeating(&CollectResources, array);
  }

  static void SetResourceFilter(DisplayResourceProvider* resource_provider,
                                ResourceId id,
                                GLenum filter) {
    DisplayResourceProvider::ScopedSamplerGL sampler(resource_provider, id,
                                                     GL_TEXTURE_2D, filter);
  }


  TransferableResource CreateResource(ResourceFormat format) {
    if (use_gpu()) {
      unsigned texture;
      child_gl_->GenTextures(1, &texture);
      gpu::Mailbox gpu_mailbox;
      child_gl_->ProduceTextureDirectCHROMIUM(texture, gpu_mailbox.name);
      gpu::SyncToken sync_token;
      child_gl_->GenSyncTokenCHROMIUM(sync_token.GetData());
      EXPECT_TRUE(sync_token.HasData());

      TransferableResource gl_resource = TransferableResource::MakeGL(
          gpu_mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token);
      gl_resource.format = format;
      return gl_resource;
    } else {
      gfx::Size size(64, 64);
      SharedBitmapId shared_bitmap_id = CreateAndFillSharedBitmap(
          shared_bitmap_manager_.get(), size, format, 0);

      return TransferableResource::MakeSoftware(shared_bitmap_id, size, format);
    }
  }

  ResourceId MakeGpuResourceAndSendToDisplay(
      char c,
      GLuint filter,
      GLuint target,
      const gpu::SyncToken& sync_token,
      DisplayResourceProvider* resource_provider) {
    ReturnCallback return_callback = base::DoNothing();

    int child = resource_provider->CreateChild(return_callback, true);

    gpu::Mailbox gpu_mailbox;
    gpu_mailbox.name[0] = c;
    gpu_mailbox.name[1] = 0;
    auto resource = TransferableResource::MakeGL(gpu_mailbox, GL_LINEAR, target,
                                                 sync_token);
    resource.id = 11;
    resource_provider->ReceiveFromChild(child, {resource});
    auto& map = resource_provider->GetChildToParentMap(child);
    return map.find(resource.id)->second;
  }

 protected:
  const bool use_gpu_;
  const bool child_needs_sync_token_;
  const std::unique_ptr<ContextSharedData> shared_data_;
  ResourceProviderGLES2Interface* gl_ = nullptr;
  ResourceProviderGLES2Interface* child_gl_ = nullptr;
  scoped_refptr<TestContextProvider> context_provider_;
  scoped_refptr<TestContextProvider> child_context_provider_;
  std::unique_ptr<TestGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<TestSharedBitmapManager> shared_bitmap_manager_;
};

INSTANTIATE_TEST_CASE_P(DisplayResourceProviderTests,
                        DisplayResourceProviderTest,
                        ::testing::Values(false, true));

TEST_P(DisplayResourceProviderTest, LockForExternalUse) {
  // TODO(penghuang): consider supporting SW mode.
  if (!use_gpu())
    return;

  gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x123),
                             0x42);
  auto mailbox = gpu::Mailbox::Generate();
  TransferableResource gl_resource = TransferableResource::MakeGL(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token1);
  ResourceId id1 = child_resource_provider_->ImportResource(
      gl_resource, SingleReleaseCallback::Create(base::DoNothing()));
  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent({id1}, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  unsigned parent_id = resource_map[list.front().id];

  DisplayResourceProvider::LockSetForExternalUse lock_set(
      resource_provider_.get());

  ResourceMetadata metadata = lock_set.LockResource(parent_id);
  ASSERT_EQ(metadata.mailbox, mailbox);
  ASSERT_TRUE(metadata.sync_token.HasData());

  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  // The resource should not be returned due to the external use lock.
  EXPECT_EQ(0u, returned_to_child.size());

  gpu::SyncToken sync_token2(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x234),
                             0x456);
  sync_token2.SetVerifyFlush();
  lock_set.UnlockResources(sync_token2);
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  // The resource should be returned after the lock is released.
  EXPECT_EQ(1u, returned_to_child.size());
  EXPECT_EQ(sync_token2, returned_to_child[0].sync_token);
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
}

TEST_P(DisplayResourceProviderTest, ReadLockCountStopsReturnToChildOrDelete) {
  if (!use_gpu())
    return;

  MockReleaseCallback release;
  TransferableResource tran = CreateResource(RGBA_8888);
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran, SingleReleaseCallback::Create(base::BindOnce(
                &MockReleaseCallback::Released, base::Unretained(&release))));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);
  {
    // Transfer some resources to the parent.
    std::vector<TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        {id1}, &list, child_context_provider_.get());
    ASSERT_EQ(1u, list.size());
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

    resource_provider_->ReceiveFromChild(child_id, list);

    // In DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<ResourceId, ResourceId> resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    ResourceId mapped_resource_id = resource_map[list[0].id];
    resource_provider_->WaitSyncToken(mapped_resource_id);
    DisplayResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                                   mapped_resource_id);

    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      ResourceIdSet());
    EXPECT_EQ(0u, returned_to_child.size());
  }

  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  // No need to wait for the sync token here -- it will be returned to the
  // client on delete.
  {
    EXPECT_CALL(release, Released(_, _));
    child_resource_provider_->RemoveImportedResource(id1);
  }

  resource_provider_->DestroyChild(child_id);
}

class TestFence : public ResourceFence {
 public:
  TestFence() = default;

  // ResourceFence implementation.
  void Set() override {}
  bool HasPassed() override { return passed; }
  void Wait() override {}

  bool passed = false;

 private:
  ~TestFence() override = default;
};

TEST_P(DisplayResourceProviderTest, ReadLockFenceStopsReturnToChildOrDelete) {
  if (!use_gpu())
    return;

  MockReleaseCallback release;
  TransferableResource tran1 = CreateResource(RGBA_8888);
  tran1.read_lock_fences_enabled = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran1, SingleReleaseCallback::Create(base::BindOnce(
                 &MockReleaseCallback::Released, base::Unretained(&release))));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent({id1}, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(list[0].read_lock_fences_enabled);

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    unsigned parent_id = resource_map[list.front().id];
    resource_provider_->WaitSyncToken(parent_id);
    DisplayResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                                   parent_id);
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

TEST_P(DisplayResourceProviderTest, ReadLockFenceDestroyChild) {
  if (!use_gpu())
    return;

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
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);

  // Transfer resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent({id1, id2}, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    for (size_t i = 0; i < list.size(); i++) {
      unsigned parent_id = resource_map[list[i].id];
      resource_provider_->WaitSyncToken(parent_id);
      DisplayResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                                     parent_id);
    }
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

TEST_P(DisplayResourceProviderTest, ReadLockFenceContextLost) {
  if (!use_gpu())
    return;

  TransferableResource tran1 = CreateResource(RGBA_8888);
  tran1.read_lock_fences_enabled = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      tran1, SingleReleaseCallback::Create(base::DoNothing()));

  TransferableResource tran2 = CreateResource(RGBA_8888);
  tran2.read_lock_fences_enabled = false;
  ResourceId id2 = child_resource_provider_->ImportResource(
      tran2, SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);

  // Transfer resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent({id1, id2}, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    for (size_t i = 0; i < list.size(); i++) {
      unsigned parent_id = resource_map[list[i].id];
      resource_provider_->WaitSyncToken(parent_id);
      DisplayResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                                     parent_id);
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

TEST_P(DisplayResourceProviderTest, ReturnResourcesWithoutSyncToken) {
  if (!use_gpu())
    return;

  auto no_token_resource_provider = std::make_unique<ClientResourceProvider>(
      /*delegated_sync_points_required=*/true);

  GLuint external_texture_id = child_gl_->CreateExternalTexture();

  // A sync point is specified directly and should be used.
  gpu::Mailbox external_mailbox;
  child_gl_->ProduceTextureDirectCHROMIUM(external_texture_id,
                                          external_mailbox.name);
  gpu::SyncToken external_sync_token;
  child_gl_->GenSyncTokenCHROMIUM(external_sync_token.GetData());
  EXPECT_TRUE(external_sync_token.HasData());
  ResourceId id = no_token_resource_provider->ImportResource(
      TransferableResource::MakeGL(external_mailbox, GL_LINEAR,
                                   GL_TEXTURE_EXTERNAL_OES,
                                   external_sync_token),
      SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), false);
  {
    // Transfer some resources to the parent.
    std::vector<TransferableResource> list;
    no_token_resource_provider->PrepareSendToParent(
        {id}, &list, child_context_provider_.get());
    ASSERT_EQ(1u, list.size());
    // A given sync point should be passed through.
    EXPECT_EQ(external_sync_token, list[0].mailbox_holder.sync_token);
    resource_provider_->ReceiveFromChild(child_id, list);

    ResourceIdSet resource_ids_to_receive;
    resource_ids_to_receive.insert(id);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_receive);
  }

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceIdSet no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(1u, returned_to_child.size());
    std::map<ResourceId, gpu::SyncToken> returned_sync_tokens;
    for (const auto& returned : returned_to_child)
      returned_sync_tokens[returned.id] = returned.sync_token;

    // Original sync point given should be returned.
    ASSERT_TRUE(returned_sync_tokens.find(id) != returned_sync_tokens.end());
    EXPECT_EQ(external_sync_token, returned_sync_tokens[id]);
    EXPECT_FALSE(returned_to_child[0].lost);
    no_token_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  resource_provider_->DestroyChild(child_id);
  no_token_resource_provider->RemoveImportedResource(id);
}

// Test that ScopedBatchReturnResources batching works.
TEST_P(DisplayResourceProviderTest, ScopedBatchReturnResourcesPreventsReturn) {
  if (!use_gpu())
    return;

  MockReleaseCallback release;

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);

  // Transfer some resources to the parent.
  constexpr size_t kTotalResources = 5;
  constexpr size_t kLockedResources = 3;
  constexpr size_t kUsedResources = 4;
  ResourceId ids[kTotalResources];
  for (size_t i = 0; i < kTotalResources; i++) {
    TransferableResource tran = CreateResource(RGBA_8888);
    ids[i] = child_resource_provider_->ImportResource(
        tran, SingleReleaseCallback::Create(base::BindOnce(
                  &MockReleaseCallback::Released, base::Unretained(&release))));
  }
  std::vector<ResourceId> resource_ids_to_transfer(ids, ids + kTotalResources);

  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(kTotalResources, list.size());
  for (const auto& id : ids)
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  std::vector<std::unique_ptr<DisplayResourceProvider::ScopedReadLockGL>>
      read_locks;
  for (size_t i = 0; i < kLockedResources; i++) {
    unsigned int mapped_resource_id = resource_map[ids[i]];
    resource_provider_->WaitSyncToken(mapped_resource_id);
    read_locks.push_back(
        std::make_unique<DisplayResourceProvider::ScopedReadLockGL>(
            resource_provider_.get(), mapped_resource_id));
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
    read_locks.clear();
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
}

TEST_P(DisplayResourceProviderTest, LostMailboxInParent) {
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  auto tran = CreateResource(RGBA_8888);
  tran.id = 11;

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child), true);

  // Receive a resource then lose the gpu context.
  resource_provider_->ReceiveFromChild(child_id, {tran});
  resource_provider_->DidLoseContextProvider();

  // Transfer resources back from the parent to the child.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, {});
  ASSERT_EQ(1u, returned_to_child.size());

  // Losing an output surface only loses hardware resources.
  EXPECT_EQ(returned_to_child[0].lost, use_gpu());
}

TEST_P(DisplayResourceProviderTest, ReadSoftwareResources) {
  if (use_gpu())
    return;

  gfx::Size size(64, 64);
  ResourceFormat format = RGBA_8888;
  const uint32_t kBadBeef = 0xbadbeef;
  SharedBitmapId shared_bitmap_id = CreateAndFillSharedBitmap(
      shared_bitmap_manager_.get(), size, format, kBadBeef);

  auto resource =
      TransferableResource::MakeSoftware(shared_bitmap_id, size, format);

  MockReleaseCallback release;
  ResourceId resource_id = child_resource_provider_->ImportResource(
      resource,
      SingleReleaseCallback::Create(base::BindOnce(
          &MockReleaseCallback::Released, base::Unretained(&release))));
  EXPECT_NE(0u, resource_id);

  // Transfer resources to the parent.
  std::vector<TransferableResource> send_to_parent;
  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child), true);
  child_resource_provider_->PrepareSendToParent({resource_id}, &send_to_parent,
                                                child_context_provider_.get());
  resource_provider_->ReceiveFromChild(child_id, send_to_parent);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceId mapped_resource_id = resource_map[resource_id];

  {
    DisplayResourceProvider::ScopedReadLockSkImage lock(
        resource_provider_.get(), mapped_resource_id);
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
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  EXPECT_CALL(release, Released(_, false));
  child_resource_provider_->RemoveImportedResource(resource_id);
}

class TextureStateTrackingGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD2(BindTexture, void(GLenum target, GLuint texture));
  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte* sync_token));
  MOCK_METHOD2(ProduceTextureDirectCHROMIUM,
               void(GLuint texture, GLbyte* mailbox));
  MOCK_METHOD1(CreateAndConsumeTextureCHROMIUM,
               unsigned(const GLbyte* mailbox));

  // Force all textures to be consecutive numbers starting at "1",
  // so we easily can test for them.
  GLuint NextTextureId() override {
    base::AutoLock lock(namespace_->lock);
    return namespace_->next_texture_id++;
  }

  void RetireTextureId(GLuint) override {}

  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                                   next_fence_sync_++);
    sync_token_data.SetVerifyFlush();
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  GLuint64 GetNextFenceSync() const { return next_fence_sync_; }

  GLuint64 next_fence_sync_ = 1;
};

class ResourceProviderTestImportedResourceGLFilters {
 public:
  static void RunTest(TestSharedBitmapManager* shared_bitmap_manager,
                      bool mailbox_nearest_neighbor,
                      GLenum sampler_filter) {
    auto gl_owned = std::make_unique<TextureStateTrackingGLES2Interface>();
    TextureStateTrackingGLES2Interface* gl = gl_owned.get();
    auto context_provider = TestContextProvider::Create(std::move(gl_owned));
    context_provider->BindToCurrentThread();

    auto resource_provider = std::make_unique<DisplayResourceProvider>(
        DisplayResourceProvider::kGpu, context_provider.get(),
        shared_bitmap_manager);

    auto child_gl_owned =
        std::make_unique<TextureStateTrackingGLES2Interface>();
    TextureStateTrackingGLES2Interface* child_gl = child_gl_owned.get();
    auto child_context_provider =
        TestContextProvider::Create(std::move(child_gl_owned));
    child_context_provider->BindToCurrentThread();

    auto child_resource_provider = std::make_unique<ClientResourceProvider>(
        /*delegated_sync_points_required=*/true);

    unsigned texture_id = 1;
    gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                              gpu::CommandBufferId::FromUnsafeValue(0x12),
                              0x34);
    const GLuint64 current_fence_sync = child_gl->GetNextFenceSync();

    EXPECT_CALL(*child_gl, BindTexture(_, _)).Times(0);
    EXPECT_CALL(*child_gl, WaitSyncTokenCHROMIUM(_)).Times(0);
    EXPECT_CALL(*child_gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);
    EXPECT_CALL(*child_gl, CreateAndConsumeTextureCHROMIUM(_)).Times(0);

    gpu::Mailbox gpu_mailbox;
    memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
    GLuint filter = mailbox_nearest_neighbor ? GL_NEAREST : GL_LINEAR;
    auto resource = TransferableResource::MakeGL(gpu_mailbox, filter,
                                                 GL_TEXTURE_2D, sync_token);

    MockReleaseCallback release;
    ResourceId resource_id = child_resource_provider->ImportResource(
        resource,
        SingleReleaseCallback::Create(base::BindOnce(
            &MockReleaseCallback::Released, base::Unretained(&release))));
    EXPECT_NE(0u, resource_id);
    EXPECT_EQ(current_fence_sync, child_gl->GetNextFenceSync());

    testing::Mock::VerifyAndClearExpectations(child_gl);

    // Transfer resources to the parent.
    std::vector<TransferableResource> send_to_parent;
    std::vector<ReturnedResource> returned_to_child;
    int child_id = resource_provider->CreateChild(
        base::BindRepeating(&CollectResources, &returned_to_child), true);
    child_resource_provider->PrepareSendToParent({resource_id}, &send_to_parent,
                                                 child_context_provider.get());
    resource_provider->ReceiveFromChild(child_id, send_to_parent);

    // In DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<ResourceId, ResourceId> resource_map =
        resource_provider->GetChildToParentMap(child_id);
    ResourceId mapped_resource_id = resource_map[resource_id];
    {
      // The verified flush flag will be set by
      // ClientResourceProvider::PrepareSendToParent. Before checking if
      // the gpu::SyncToken matches, set this flag first.
      sync_token.SetVerifyFlush();

      // Mailbox sync point WaitSyncToken before using the texture.
      EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(MatchesSyncToken(sync_token)));
      resource_provider->WaitSyncToken(mapped_resource_id);
      testing::Mock::VerifyAndClearExpectations(gl);

      EXPECT_CALL(*gl, CreateAndConsumeTextureCHROMIUM(_))
          .WillOnce(Return(texture_id));
      EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, texture_id));

      EXPECT_CALL(*gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);

      // The sampler will reset these if |mailbox_nearest_neighbor| does not
      // match |sampler_filter|.
      if (mailbox_nearest_neighbor != (sampler_filter == GL_NEAREST)) {
        EXPECT_CALL(*gl, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                       sampler_filter));
        EXPECT_CALL(*gl, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                       sampler_filter));
      }

      DisplayResourceProvider::ScopedSamplerGL lock(
          resource_provider.get(), mapped_resource_id, sampler_filter);
      testing::Mock::VerifyAndClearExpectations(gl);
      EXPECT_EQ(current_fence_sync, gl->GetNextFenceSync());

      // When done with it, a sync point should be inserted, but no produce is
      // necessary.
      EXPECT_CALL(*child_gl, BindTexture(_, _)).Times(0);
      EXPECT_CALL(*child_gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);

      EXPECT_CALL(*child_gl, WaitSyncTokenCHROMIUM(_)).Times(0);
      EXPECT_CALL(*child_gl, CreateAndConsumeTextureCHROMIUM(_)).Times(0);
    }

    EXPECT_EQ(0u, returned_to_child.size());
    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    resource_provider->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
    EXPECT_EQ(1u, returned_to_child.size());
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);

    gpu::SyncToken released_sync_token;
    {
      EXPECT_CALL(release, Released(_, false))
          .WillOnce(SaveArg<0>(&released_sync_token));
      child_resource_provider->RemoveImportedResource(resource_id);
    }
    EXPECT_TRUE(released_sync_token.HasData());
  }
};

TEST_P(DisplayResourceProviderTest, ReceiveGLTexture2D_LinearToLinear) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), false, GL_LINEAR);
}

TEST_P(DisplayResourceProviderTest, ReceiveGLTexture2D_NearestToNearest) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), true, GL_NEAREST);
}

TEST_P(DisplayResourceProviderTest, ReceiveGLTexture2D_NearestToLinear) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), true, GL_LINEAR);
}

TEST_P(DisplayResourceProviderTest, ReceiveGLTexture2D_LinearToNearest) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), false, GL_NEAREST);
}

TEST_P(DisplayResourceProviderTest, ReceiveGLTextureExternalOES) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto gl_owned = std::make_unique<TextureStateTrackingGLES2Interface>();
  TextureStateTrackingGLES2Interface* gl = gl_owned.get();
  auto context_provider = TestContextProvider::Create(std::move(gl_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider = std::make_unique<DisplayResourceProvider>(
      DisplayResourceProvider::kGpu, context_provider.get(),
      shared_bitmap_manager_.get());

  auto child_gl_owned = std::make_unique<TextureStateTrackingGLES2Interface>();
  TextureStateTrackingGLES2Interface* child_gl = child_gl_owned.get();
  auto child_context_provider =
      TestContextProvider::Create(std::move(child_gl_owned));
  child_context_provider->BindToCurrentThread();

  auto child_resource_provider = std::make_unique<ClientResourceProvider>(
      /*delegated_sync_points_required=*/true);

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  const GLuint64 current_fence_sync = child_gl->GetNextFenceSync();

  EXPECT_CALL(*child_gl, BindTexture(_, _)).Times(0);
  EXPECT_CALL(*child_gl, WaitSyncTokenCHROMIUM(_)).Times(0);
  EXPECT_CALL(*child_gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*child_gl, CreateAndConsumeTextureCHROMIUM(_)).Times(0);

  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  std::unique_ptr<SingleReleaseCallback> callback =
      SingleReleaseCallback::Create(base::DoNothing());

  auto resource = TransferableResource::MakeGL(
      gpu_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES, sync_token);

  ResourceId resource_id =
      child_resource_provider->ImportResource(resource, std::move(callback));
  EXPECT_NE(0u, resource_id);
  EXPECT_EQ(current_fence_sync, child_gl->GetNextFenceSync());

  testing::Mock::VerifyAndClearExpectations(child_gl);

  // Transfer resources to the parent.
  std::vector<TransferableResource> send_to_parent;
  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child), true);
  child_resource_provider->PrepareSendToParent({resource_id}, &send_to_parent,
                                               child_context_provider_.get());
  resource_provider->ReceiveFromChild(child_id, send_to_parent);

  // Before create DrawQuad in DisplayResourceProvider's namespace, get the
  // mapped resource id first.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider->GetChildToParentMap(child_id);
  ResourceId mapped_resource_id = resource_map[resource_id];
  {
    // The verified flush flag will be set by
    // ClientResourceProvider::PrepareSendToParent. Before checking if
    // the gpu::SyncToken matches, set this flag first.
    sync_token.SetVerifyFlush();

    // Mailbox sync point WaitSyncToken before using the texture.
    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(MatchesSyncToken(sync_token)));
    resource_provider->WaitSyncToken(mapped_resource_id);
    testing::Mock::VerifyAndClearExpectations(gl);

    unsigned texture_id = 1;

    EXPECT_CALL(*gl, CreateAndConsumeTextureCHROMIUM(_))
        .WillOnce(Return(texture_id));

    EXPECT_CALL(*gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);

    DisplayResourceProvider::ScopedReadLockGL lock(resource_provider.get(),
                                                   mapped_resource_id);
    testing::Mock::VerifyAndClearExpectations(gl);

    // When done with it, a sync point should be inserted, but no produce is
    // necessary.
    EXPECT_CALL(*gl, BindTexture(_, _)).Times(0);
    EXPECT_CALL(*gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);

    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(_)).Times(0);
    EXPECT_CALL(*gl, CreateAndConsumeTextureCHROMIUM(_)).Times(0);
    testing::Mock::VerifyAndClearExpectations(gl);
  }
  EXPECT_EQ(0u, returned_to_child.size());
  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  resource_provider->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider->ReceiveReturnsFromParent(returned_to_child);

  child_resource_provider->RemoveImportedResource(resource_id);
}

TEST_P(DisplayResourceProviderTest, WaitSyncTokenIfNeeded) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto gl_owned = std::make_unique<TextureStateTrackingGLES2Interface>();
  TextureStateTrackingGLES2Interface* gl = gl_owned.get();
  auto context_provider = TestContextProvider::Create(std::move(gl_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider = std::make_unique<DisplayResourceProvider>(
      DisplayResourceProvider::kGpu, context_provider.get(),
      shared_bitmap_manager_.get());

  const GLuint64 current_fence_sync = gl->GetNextFenceSync();

  EXPECT_CALL(*gl, BindTexture(_, _)).Times(0);
  EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(_)).Times(0);
  EXPECT_CALL(*gl, ProduceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*gl, CreateAndConsumeTextureCHROMIUM(_)).Times(0);

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  ResourceId id_with_sync = MakeGpuResourceAndSendToDisplay(
      'a', GL_LINEAR, GL_TEXTURE_2D, sync_token, resource_provider.get());
  ResourceId id_without_sync = MakeGpuResourceAndSendToDisplay(
      'a', GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(), resource_provider.get());

  EXPECT_EQ(current_fence_sync, gl->GetNextFenceSync());

  // First call to WaitSyncToken should call WaitSyncToken, but only if a
  // SyncToken was present.
  {
    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(MatchesSyncToken(sync_token)))
        .Times(1);
    resource_provider->WaitSyncToken(id_with_sync);
    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(_)).Times(0);
    resource_provider->WaitSyncToken(id_without_sync);
  }

  {
    // Subsequent calls to WaitSyncToken shouldn't call WaitSyncToken.
    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(_)).Times(0);
    resource_provider->WaitSyncToken(id_with_sync);
    resource_provider->WaitSyncToken(id_without_sync);
  }
}

#if defined(OS_ANDROID)
TEST_P(DisplayResourceProviderTest, OverlayPromotionHint) {
  if (!use_gpu())
    return;

  GLuint external_texture_id = child_gl_->CreateExternalTexture();

  gpu::Mailbox external_mailbox;
  child_gl_->ProduceTextureDirectCHROMIUM(external_texture_id,
                                          external_mailbox.name);
  gpu::SyncToken external_sync_token;
  child_gl_->GenSyncTokenCHROMIUM(external_sync_token.GetData());
  EXPECT_TRUE(external_sync_token.HasData());

  TransferableResource id1_transfer = TransferableResource::MakeGLOverlay(
      external_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES, external_sync_token,
      gfx::Size(1, 1), true);
  id1_transfer.wants_promotion_hint = true;
  id1_transfer.is_backed_by_surface_texture = true;
  ResourceId id1 = child_resource_provider_->ImportResource(
      id1_transfer, SingleReleaseCallback::Create(base::DoNothing()));

  TransferableResource id2_transfer = TransferableResource::MakeGLOverlay(
      external_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES, external_sync_token,
      gfx::Size(1, 1), true);
  id2_transfer.wants_promotion_hint = false;
  id2_transfer.is_backed_by_surface_texture = false;
  ResourceId id2 = child_resource_provider_->ImportResource(
      id2_transfer, SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = resource_provider_->CreateChild(
      GetReturnCallback(&returned_to_child), true);

  // Transfer some resources to the parent.
  std::vector<TransferableResource> list;
  child_resource_provider_->PrepareSendToParent({id1, id2}, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  resource_provider_->ReceiveFromChild(child_id, list);
  std::unordered_map<ResourceId, ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceId mapped_id1 = resource_map[list[0].id];
  ResourceId mapped_id2 = resource_map[list[1].id];

  // The promotion hints should not be recorded until after we wait.  This is
  // because we can't notify them until they're synchronized, and we choose to
  // ignore unwaited resources rather than send them a "no" hint.  If they end
  // up in the request set before we wait, then the attempt to notify them wil;
  // DCHECK when we try to lock them for reading in SendPromotionHints.
  EXPECT_EQ(0u, resource_provider_->CountPromotionHintRequestsForTesting());
  {
    resource_provider_->WaitSyncToken(mapped_id1);
    DisplayResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                                   mapped_id1);
  }
  EXPECT_EQ(1u, resource_provider_->CountPromotionHintRequestsForTesting());

  EXPECT_EQ(list[0].mailbox_holder.sync_token, gl_->last_waited_sync_token());
  ResourceIdSet resource_ids_to_receive;
  resource_ids_to_receive.insert(id1);
  resource_ids_to_receive.insert(id2);
  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    resource_ids_to_receive);

  EXPECT_EQ(2u, resource_provider_->num_resources());

  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);

  // Make sure that the request for a promotion hint was noticed.
  EXPECT_TRUE(resource_provider_->IsOverlayCandidate(mapped_id1));
  EXPECT_TRUE(resource_provider_->IsBackedBySurfaceTexture(mapped_id1));
  EXPECT_TRUE(resource_provider_->WantsPromotionHintForTesting(mapped_id1));

  EXPECT_TRUE(resource_provider_->IsOverlayCandidate(mapped_id2));
  EXPECT_FALSE(resource_provider_->IsBackedBySurfaceTexture(mapped_id2));
  EXPECT_FALSE(resource_provider_->WantsPromotionHintForTesting(mapped_id2));

  // ResourceProvider maintains a set of promotion hint requests that should be
  // cleared when resources are deleted.
  resource_provider_->DeclareUsedResourcesFromChild(child_id, ResourceIdSet());
  EXPECT_EQ(2u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  EXPECT_EQ(0u, resource_provider_->CountPromotionHintRequestsForTesting());

  resource_provider_->DestroyChild(child_id);

  child_resource_provider_->RemoveImportedResource(id2);
  child_resource_provider_->RemoveImportedResource(id1);
}
#endif

}  // namespace
}  // namespace viz

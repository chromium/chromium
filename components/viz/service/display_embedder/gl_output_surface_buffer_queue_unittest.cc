// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue.h"

#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/buffer_queue.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/surface_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/swap_result.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::InSequence;
using testing::Mock;
using testing::Ne;
using testing::NotNull;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace viz {

class MockGLES2Interface : public TestGLES2Interface {
 public:
  MockGLES2Interface() = default;
  ~MockGLES2Interface() override = default;

  MOCK_METHOD2(DeleteTextures, void(GLsizei, const GLuint*));
  MOCK_METHOD2(BindFramebuffer, void(GLenum, GLuint));
  MOCK_METHOD2(GenRenderbuffers, void(GLsizei, GLuint*));
  MOCK_METHOD2(BindRenderbuffer, void(GLenum, GLuint));
  MOCK_METHOD2(DeleteRenderbuffers, void(GLsizei n, const GLuint*));
  MOCK_METHOD1(CreateAndTexStorage2DSharedImageCHROMIUM, GLuint(const GLbyte*));
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte*));
  MOCK_METHOD2(BeginSharedImageAccessDirectCHROMIUM, void(GLuint, GLenum));
  MOCK_METHOD1(EndSharedImageAccessDirectCHROMIUM, void(GLuint));
};

class MockBufferQueue : public BufferQueue {
 public:
  MockBufferQueue()
      : BufferQueue(/*sii_=*/nullptr,
                    gpu::kNullSurfaceHandle) {}
  ~MockBufferQueue() override = default;

  MOCK_METHOD1(GetCurrentBuffer, gpu::Mailbox(gpu::SyncToken*));
  MOCK_CONST_METHOD0(CurrentBufferDamage, gfx::Rect());
  MOCK_METHOD1(SwapBuffers, void(const gfx::Rect&));
  MOCK_METHOD0(PageFlipComplete, void());
  MOCK_METHOD0(FreeAllSurfaces, void());
  MOCK_METHOD3(Reshape,
               bool(const gfx::Size&,
                    const gfx::ColorSpace&,
                    gfx::BufferFormat));

  MOCK_METHOD0(DoSetSyncTokenProvider, void());
  void SetSyncTokenProvider(SyncTokenProvider* sync_token_provider) override {
    BufferQueue::SetSyncTokenProvider(sync_token_provider);
    DoSetSyncTokenProvider();
  }
};

class GLOutputSurfaceBufferQueueTest : public ::testing::Test,
                                       public OutputSurfaceClient {
 public:
  GLOutputSurfaceBufferQueueTest() = default;
  ~GLOutputSurfaceBufferQueueTest() override = default;

  void SetUp() override {
    auto buffer_queue = std::make_unique<StrictMock<MockBufferQueue>>();
    buffer_queue_ = buffer_queue.get();

    auto gles2_interface = std::make_unique<StrictMock<MockGLES2Interface>>();
    gles2_interface_ = gles2_interface.get();

    EXPECT_CALL(*buffer_queue_, DoSetSyncTokenProvider());
    surface_ = std::make_unique<GLOutputSurfaceBufferQueue>(
        base::MakeRefCounted<TestVizProcessContextProvider>(
            std::make_unique<TestContextSupport>(), std::move(gles2_interface)),
        gpu::kNullSurfaceHandle, std::move(buffer_queue));
    surface_->BindToClient(this);

    Mock::VerifyAndClearExpectations(gles2_interface_);
    Mock::VerifyAndClearExpectations(buffer_queue_);
  }

  // OutputSurfaceClient implementation.
  void DidReceiveSwapBuffersAck(const gfx::SwapTimings& timings) override {}
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override {}
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override {}
  void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DidSwapWithSize(const gfx::Size& pixel_size) override {}
  void DidReceivePresentationFeedback(
      const gfx::PresentationFeedback& feedback) override {}
  void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) override {}

 protected:
  std::unique_ptr<OutputSurface> surface_;
  StrictMock<MockGLES2Interface>* gles2_interface_;
  StrictMock<MockBufferQueue>* buffer_queue_;
};

MATCHER_P(SyncTokenEqualTo, expected_sync_token, "") {
  auto* actual_sync_token = reinterpret_cast<const gpu::SyncToken*>(arg);
  return expected_sync_token == *actual_sync_token;
}

MATCHER_P(SharedImageEqualTo, expected_shared_image, "") {
  gpu::Mailbox actual_shared_image;
  actual_shared_image.SetName(arg);
  return expected_shared_image == actual_shared_image;
}

// Make sure that the surface uses the buffer queue and the GL context correctly
// when we request it to bind the framebuffer twice and then swap the buffer.
TEST_F(GLOutputSurfaceBufferQueueTest, BindFramebufferAndSwap) {
  const gpu::SyncToken fake_sync_token(
      gpu::CommandBufferNamespace::GPU_IO,
      gpu::CommandBufferId::FromUnsafeValue(567u),
      /*release_count=*/5u);
  const gpu::Mailbox fake_shared_image = gpu::Mailbox::GenerateForSharedImage();
  constexpr GLuint kFakeTexture = 123u;
  {
    InSequence dummy_sequence;

    // The first call to |surface_|->BindFramebuffer() should result in binding
    // the GL framebuffer, requesting a new buffer, waiting on the corresponding
    // sync token, and beginning read/write access to the shared image.
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Ne(0u)));
    EXPECT_CALL(*buffer_queue_, GetCurrentBuffer(NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(fake_sync_token),
                        Return(fake_shared_image)));
    EXPECT_CALL(*gles2_interface_,
                WaitSyncTokenCHROMIUM(SyncTokenEqualTo(fake_sync_token)));
    EXPECT_CALL(*gles2_interface_, CreateAndTexStorage2DSharedImageCHROMIUM(
                                       SharedImageEqualTo(fake_shared_image)))
        .WillOnce(Return(kFakeTexture));
    EXPECT_CALL(
        *gles2_interface_,
        BeginSharedImageAccessDirectCHROMIUM(
            kFakeTexture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM));

    // The second call to |surface_|->BindFramebuffer() should only result in
    // binding the GL framebuffer because the underlying buffer hasn't been
    // swapped.
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Ne(0u)));

    // Calling |surface_|->SwapBuffers() should result in ending read/write
    // access to the underlying buffer and unbinding the GL framebuffer.
    EXPECT_CALL(*gles2_interface_,
                EndSharedImageAccessDirectCHROMIUM(kFakeTexture));
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Eq(0u)));
    EXPECT_CALL(*buffer_queue_, SwapBuffers(_));

    // Destroying |surface_| should result in the deletion of the texture
    // obtained from consuming the shared image.
    EXPECT_CALL(*gles2_interface_,
                DeleteTextures(1u, Pointee(Eq(kFakeTexture))));
  }

  surface_->BindFramebuffer();
  surface_->BindFramebuffer();
  surface_->SwapBuffers(OutputSurfaceFrame());
}

TEST_F(GLOutputSurfaceBufferQueueTest, EmptySwap) {
  const gpu::SyncToken fake_sync_token(
      gpu::CommandBufferNamespace::GPU_IO,
      gpu::CommandBufferId::FromUnsafeValue(567u),
      /*release_count=*/5u);
  const gpu::Mailbox fake_shared_image = gpu::Mailbox::GenerateForSharedImage();
  constexpr GLuint kFakeTexture = 123u;
  {
    InSequence dummy_sequence;

    // The call to |surface_|->BindFramebuffer() should result in binding the GL
    // framebuffer, requesting a new buffer, waiting on the corresponding sync
    // token, and beginning read/write access to the shared image.
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Ne(0u)));
    EXPECT_CALL(*buffer_queue_, GetCurrentBuffer(NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(fake_sync_token),
                        Return(fake_shared_image)));
    EXPECT_CALL(*gles2_interface_,
                WaitSyncTokenCHROMIUM(SyncTokenEqualTo(fake_sync_token)));
    EXPECT_CALL(*gles2_interface_, CreateAndTexStorage2DSharedImageCHROMIUM(
                                       SharedImageEqualTo(fake_shared_image)))
        .WillOnce(Return(kFakeTexture));
    EXPECT_CALL(
        *gles2_interface_,
        BeginSharedImageAccessDirectCHROMIUM(
            kFakeTexture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM));

    // The first call to |surface_|->SwapBuffers() should result in ending
    // read/write access to the underlying buffer and unbinding the GL
    // framebuffer.
    EXPECT_CALL(*gles2_interface_,
                EndSharedImageAccessDirectCHROMIUM(kFakeTexture));
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Eq(0u)));
    EXPECT_CALL(*buffer_queue_, SwapBuffers(_));

    // The two empty swaps should only result in telling the buffer queue to
    // swap the buffers.
    EXPECT_CALL(*buffer_queue_, SwapBuffers(_)).Times(2);

    // Destroying |surface_| should result in the deletion of the texture
    // obtained from consuming the shared image.
    EXPECT_CALL(*gles2_interface_,
                DeleteTextures(1u, Pointee(Eq(kFakeTexture))));
  }
  surface_->BindFramebuffer();
  unsigned texture_for_first_buffer = surface_->GetOverlayTextureId();
  EXPECT_GT(texture_for_first_buffer, 0u);
  surface_->SwapBuffers(OutputSurfaceFrame());

  // Now do two empty swaps (which don't call BindFramebuffer()).
  EXPECT_EQ(texture_for_first_buffer, surface_->GetOverlayTextureId());
  surface_->SwapBuffers(OutputSurfaceFrame());
  EXPECT_EQ(texture_for_first_buffer, surface_->GetOverlayTextureId());
  surface_->SwapBuffers(OutputSurfaceFrame());
}

// Make sure that receiving a swap NAK doesn't cause us to leak resources.
TEST_F(GLOutputSurfaceBufferQueueTest, HandleSwapNAK) {
  const gpu::SyncToken fake_sync_token(
      gpu::CommandBufferNamespace::GPU_IO,
      gpu::CommandBufferId::FromUnsafeValue(567u),
      /*release_count=*/5u);
  constexpr gfx::Size kBufferSize(100, 100);
  const gpu::Mailbox fake_shared_image = gpu::Mailbox::GenerateForSharedImage();
  constexpr GLuint kFakeTexture = 123u;
  constexpr GLuint kFakeStencilBuffer = 456u;
  {
    InSequence dummy_sequence;

    EXPECT_CALL(*buffer_queue_, Reshape(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Ne(0u)));

    // The call to |surface_|->BindFramebuffer() should result in binding the GL
    // framebuffer, requesting a new buffer, waiting on the corresponding sync
    // token, beginning read/write access to the shared image, and creating a
    // stencil buffer.
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Ne(0u)));
    EXPECT_CALL(*buffer_queue_, GetCurrentBuffer(NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(fake_sync_token),
                        Return(fake_shared_image)));

    EXPECT_CALL(*gles2_interface_,
                WaitSyncTokenCHROMIUM(SyncTokenEqualTo(fake_sync_token)));
    EXPECT_CALL(*gles2_interface_, CreateAndTexStorage2DSharedImageCHROMIUM(
                                       SharedImageEqualTo(fake_shared_image)))
        .WillOnce(Return(kFakeTexture));
    EXPECT_CALL(
        *gles2_interface_,
        BeginSharedImageAccessDirectCHROMIUM(
            kFakeTexture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM));
    EXPECT_CALL(*gles2_interface_, GenRenderbuffers(1u, NotNull()))
        .WillOnce(SetArgPointee<1>(kFakeStencilBuffer));
    EXPECT_CALL(*gles2_interface_,
                BindRenderbuffer(GL_RENDERBUFFER, kFakeStencilBuffer));
    EXPECT_CALL(*gles2_interface_, BindRenderbuffer(GL_RENDERBUFFER, 0u));

    // Calling |surface_|->SwapBuffers() should result in ending read/write
    // access to the underlying buffer and unbinding the GL framebuffer.
    EXPECT_CALL(*gles2_interface_,
                EndSharedImageAccessDirectCHROMIUM(kFakeTexture));
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Eq(0u)));
    EXPECT_CALL(*buffer_queue_, SwapBuffers(_));

    // Receiving a swap NAK should result in the deletion of the texture
    // obtained from consuming the shared image. It should also result in the
    // deletion of the stencil buffer.
    EXPECT_CALL(*buffer_queue_, FreeAllSurfaces());
    EXPECT_CALL(*gles2_interface_, BindFramebuffer(_, Ne(0u)));
    EXPECT_CALL(*gles2_interface_,
                DeleteRenderbuffers(1u, Pointee(Eq(kFakeStencilBuffer))));
    EXPECT_CALL(*gles2_interface_,
                DeleteTextures(1u, Pointee(Eq(kFakeTexture))));
    EXPECT_CALL(*buffer_queue_, PageFlipComplete());
  }

  surface_->Reshape(kBufferSize, /*device_scale_factor=*/1.0,
                    gfx::ColorSpace::CreateSRGB(), gfx::BufferFormat::BGRA_8888,
                    /*use_stencil=*/true);
  surface_->BindFramebuffer();
  OutputSurfaceFrame frame;
  frame.size = kBufferSize;
  surface_->SwapBuffers(std::move(frame));
  gfx::SwapResponse swap_response{};
  swap_response.result = gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;
  (static_cast<GLOutputSurfaceBufferQueue*>(surface_.get()))
      ->DidReceiveSwapBuffersAck(swap_response);
}

}  // namespace viz

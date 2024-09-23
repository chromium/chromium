// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/test/test_raster_interface.h"

#include <limits>
#include <utility>

#include "base/notreached.h"
#include "base/time/time.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/constants.h"

namespace viz {

TestRasterInterface::TestRasterInterface() {
  caps_.max_texture_size = 2048;
}

TestRasterInterface::~TestRasterInterface() = default;

void TestRasterInterface::Finish() {
  if (test_support_)
    test_support_->CallAllSyncPointCallbacks();
}

void TestRasterInterface::Flush() {
  if (test_support_)
    test_support_->CallAllSyncPointCallbacks();
}

GLenum TestRasterInterface::GetError() {
  return 0;
}

GLenum TestRasterInterface::GetGraphicsResetStatusKHR() {
  if (context_lost_)
    return GL_UNKNOWN_CONTEXT_RESET_KHR;
  return GL_NO_ERROR;
}

void TestRasterInterface::LoseContextCHROMIUM(GLenum current, GLenum other) {
  if (context_lost_)
    return;

  context_lost_ = true;
  if (context_lost_callback_)
    std::move(context_lost_callback_).Run();
}

void TestRasterInterface::GenQueriesEXT(GLsizei n, GLuint* queries) {
  for (GLsizei i = 0; i < n; ++i) {
    queries[i] = 1u;
  }
}

void TestRasterInterface::DeleteQueriesEXT(GLsizei n, const GLuint* queries) {}
void TestRasterInterface::BeginQueryEXT(GLenum target, GLuint id) {}
void TestRasterInterface::EndQueryEXT(GLenum target) {}
void TestRasterInterface::QueryCounterEXT(GLuint id, GLenum target) {}

void TestRasterInterface::GetQueryObjectuivEXT(GLuint id,
                                               GLenum pname,
                                               GLuint* params) {
  // If the context is lost, behave as if result is available.
  if (pname == GL_QUERY_RESULT_AVAILABLE_EXT ||
      pname == GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT) {
    *params = 1;
  }
}

void TestRasterInterface::GetQueryObjectui64vEXT(GLuint id,
                                                 GLenum pname,
                                                 GLuint64* params) {
  // This is used for testing GL_COMMANDS_ISSUED_TIMESTAMP_QUERY, so we return
  // the maximum that base::TimeDelta()::InMicroseconds() could return.
  if (pname == GL_QUERY_RESULT_EXT) {
    static_assert(std::is_same<decltype(base::TimeDelta().InMicroseconds()),
                               int64_t>::value,
                  "Expected the return type of "
                  "base::TimeDelta()::InMicroseconds() to be int64_t");
    *params = std::numeric_limits<int64_t>::max();
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

gpu::SyncToken TestRasterInterface::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    uint32_t transfer_cache_entry_id,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  return gpu::SyncToken();
}

GLuint TestRasterInterface::CreateAndConsumeForGpuRaster(
    const gpu::Mailbox& mailbox) {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

GLuint TestRasterInterface::CreateAndConsumeForGpuRaster(
    const scoped_refptr<gpu::ClientSharedImage>& shared_image) {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

void TestRasterInterface::DeleteGpuRasterTexture(GLuint texture) {
  NOTREACHED_IN_MIGRATION();
}

void TestRasterInterface::BeginGpuRaster() {
  NOTREACHED_IN_MIGRATION();
}

void TestRasterInterface::EndGpuRaster() {
  NOTREACHED_IN_MIGRATION();
}

void TestRasterInterface::BeginSharedImageAccessDirectCHROMIUM(GLuint texture,
                                                               GLenum mode) {
  NOTREACHED_IN_MIGRATION();
}

void TestRasterInterface::EndSharedImageAccessDirectCHROMIUM(GLuint texture) {
  NOTREACHED_IN_MIGRATION();
}

void TestRasterInterface::InitializeDiscardableTextureCHROMIUM(GLuint texture) {
  NOTREACHED_IN_MIGRATION();
}

void TestRasterInterface::UnlockDiscardableTextureCHROMIUM(GLuint texture) {
  NOTREACHED_IN_MIGRATION();
}

bool TestRasterInterface::LockDiscardableTextureCHROMIUM(GLuint texture) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void TestRasterInterface::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  // Don't return a valid sync token if context is lost. This matches behavior
  // of CommandBufferProxyImpl.
  if (context_lost_)
    return;

  gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                 gpu::CommandBufferId(),
                                 next_insert_fence_sync_++);
  sync_token_data.SetVerifyFlush();
  memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
}

void TestRasterInterface::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {
  // Don't return a valid sync token if context is lost. This matches behavior
  // of CommandBufferProxyImpl.
  if (context_lost_)
    return;

  gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                 gpu::CommandBufferId(),
                                 next_insert_fence_sync_++);
  memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
}

void TestRasterInterface::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                   GLsizei count) {
  for (GLsizei i = 0; i < count; ++i) {
    gpu::SyncToken sync_token_data;
    memcpy(sync_token_data.GetData(), sync_tokens[i], sizeof(sync_token_data));
    sync_token_data.SetVerifyFlush();
    memcpy(sync_tokens[i], &sync_token_data, sizeof(sync_token_data));
  }
}

void TestRasterInterface::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  gpu::SyncToken sync_token_data;
  if (sync_token)
    memcpy(&sync_token_data, sync_token, sizeof(sync_token_data));

  if (sync_token_data.release_count() >
      last_waited_sync_token_.release_count()) {
    last_waited_sync_token_ = sync_token_data;
  }
}

void TestRasterInterface::ShallowFlushCHROMIUM() {
  if (test_support_)
    test_support_->CallAllSyncPointCallbacks();
}

void TestRasterInterface::set_supports_gpu_memory_buffer_format(
    gfx::BufferFormat format,
    bool support) {
  if (support) {
    caps_.gpu_memory_buffer_formats.Put(format);
  } else {
    caps_.gpu_memory_buffer_formats.Remove(format);
  }
}

bool TestRasterInterface::ReadbackImagePixels(
    const gpu::Mailbox& source_mailbox,
    const SkImageInfo& dst_info,
    GLuint dst_row_bytes,
    int src_x,
    int src_y,
    int plane_index,
    void* dst_pixels) {
  auto size = dst_info.computeByteSize(dst_row_bytes);
  memset(dst_pixels, 0, size);
  return true;
}
}  // namespace viz

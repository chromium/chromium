// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_RASTER_INTERFACE_H_
#define COMPONENTS_VIZ_TEST_TEST_RASTER_INTERFACE_H_

#include <utility>

#include "base/functional/callback.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace viz {

// A fake implementation of RasterInterface for use in unit tests that don't
// draw anything.
class TestRasterInterface : public gpu::raster::RasterInterface {
 public:
  TestRasterInterface();
  ~TestRasterInterface() override;

  const gpu::Capabilities& capabilities() const { return caps_; }

  gpu::SyncToken last_waited_sync_token() const {
    return last_waited_sync_token_;
  }

  void set_context_lost_callback(base::OnceClosure callback) {
    context_lost_callback_ = std::move(callback);
  }
  void set_test_support(TestContextSupport* test_support) {
    test_support_ = test_support;
  }

  void set_context_lost(bool context_lost) { context_lost_ = context_lost; }

  // Capability setters below here.
  void set_gpu_rasterization(bool gpu_rasterization) {
    caps_.gpu_rasterization = gpu_rasterization;
  }
  void set_msaa_is_slow(bool msaa_is_slow) {
    caps_.msaa_is_slow = msaa_is_slow;
  }
  void set_avoid_stencil_buffers(bool avoid_stencil_buffers) {
    caps_.avoid_stencil_buffers = avoid_stencil_buffers;
  }
  void set_max_texture_size(int max_texture_size) {
    caps_.max_texture_size = max_texture_size;
  }
  void set_supports_gpu_memory_buffer_format(gfx::BufferFormat format,
                                             bool support);

  // gpu::raster::RasterInterface implementation.
  void Finish() override;
  void Flush() override;
  void OrderingBarrierCHROMIUM() override {}
  GLenum GetError() override;
  GLenum GetGraphicsResetStatusKHR() override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;
  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void QueryCounterEXT(GLuint id, GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;
  void GetQueryObjectui64vEXT(GLuint id,
                              GLenum pname,
                              GLuint64* params) override;
  void CopySharedImage(const gpu::Mailbox& source_mailbox,
                       const gpu::Mailbox& dest_mailbox,
                       GLenum dest_target,
                       GLint xoffset,
                       GLint yoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height,
                       GLboolean unpack_flip_y,
                       GLboolean unpack_premultiply_alpha) override {}
  void WritePixels(const gpu::Mailbox& dest_mailbox,
                   int dst_x_offset,
                   int dst_y_offset,
                   GLenum texture_target,
                   const SkPixmap& src_sk_pixmap) override {}
  void WritePixelsYUV(const gpu::Mailbox& dest_mailbox,
                      const SkYUVAPixmaps& src_yuv_pixmap) override {}
  void BeginRasterCHROMIUM(SkColor4f sk_color_4f,
                           GLboolean needs_clear,
                           GLuint msaa_sample_count,
                           gpu::raster::MsaaMode msaa_mode,
                           GLboolean can_use_lcd_text,
                           GLboolean visible,
                           const gfx::ColorSpace& color_space,
                           float hdr_headroom,
                           const GLbyte* mailbox) override {}
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      const gfx::Vector2dF& post_scale,
                      bool requires_clear,
                      const ScrollOffsetMap* raster_inducing_scroll_offsets,
                      size_t* max_op_size_hint) override {}
  void EndRasterCHROMIUM() override {}
  gpu::SyncToken ScheduleImageDecode(base::span<const uint8_t> encoded_data,
                                     const gfx::Size& output_size,
                                     uint32_t transfer_cache_entry_id,
                                     const gfx::ColorSpace& target_color_space,
                                     bool needs_mips) override;
  void ReadbackARGBPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      GrSurfaceOrigin source_origin,
      const gfx::Size& source_size,
      const gfx::Point& source_starting_point,
      const SkImageInfo& dst_info,
      GLuint dst_row_bytes,
      unsigned char* out,
      base::OnceCallback<void(bool)> readback_done) override {}
  void ReadbackYUVPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      const gfx::Size& source_size,
      const gfx::Rect& output_rect,
      bool vertically_flip_texture,
      int y_plane_row_stride_bytes,
      unsigned char* y_plane_data,
      int u_plane_row_stride_bytes,
      unsigned char* u_plane_data,
      int v_plane_row_stride_bytes,
      unsigned char* v_plane_data,
      const gfx::Point& paste_location,
      base::OnceCallback<void()> release_mailbox,
      base::OnceCallback<void(bool)> readback_done) override {}
  bool ReadbackImagePixels(const gpu::Mailbox& source_mailbox,
                           const SkImageInfo& dst_info,
                           GLuint dst_row_bytes,
                           int src_x,
                           int src_y,
                           int plane_index,
                           void* dst_pixels) override;
  GLuint CreateAndConsumeForGpuRaster(const gpu::Mailbox& mailbox) override;
  GLuint CreateAndConsumeForGpuRaster(
      const scoped_refptr<gpu::ClientSharedImage>& shared_image) override;
  void DeleteGpuRasterTexture(GLuint texture) override;
  void BeginGpuRaster() override;
  void EndGpuRaster() override;
  void BeginSharedImageAccessDirectCHROMIUM(GLuint texture,
                                            GLenum mode) override;
  void EndSharedImageAccessDirectCHROMIUM(GLuint texture) override;
  void InitializeDiscardableTextureCHROMIUM(GLuint texture) override;
  void UnlockDiscardableTextureCHROMIUM(GLuint texture) override;
  bool LockDiscardableTextureCHROMIUM(GLuint texture) override;
  void TraceBeginCHROMIUM(const char* category_name,
                          const char* trace_name) override {}
  void TraceEndCHROMIUM() override {}
  void SetActiveURLCHROMIUM(const char* url) override {}

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;
  void ShallowFlushCHROMIUM() override;

 private:
  gpu::Capabilities caps_;
  base::OnceClosure context_lost_callback_;
  raw_ptr<TestContextSupport> test_support_ = nullptr;

  bool context_lost_ = false;
  uint64_t next_insert_fence_sync_ = 1;
  gpu::SyncToken last_waited_sync_token_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_RASTER_INTERFACE_H_

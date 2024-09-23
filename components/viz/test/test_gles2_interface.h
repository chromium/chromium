// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_
#define COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_

#include <stddef.h>

#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

class TestContextSupport;

class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  TestGLES2Interface();
  ~TestGLES2Interface() override;

  // Overridden from gpu::gles2::GLES2Interface
  void GenTextures(GLsizei n, GLuint* textures) override;
  void GenBuffers(GLsizei n, GLuint* buffers) override;
  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override;
  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override;
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;

  void DeleteTextures(GLsizei n, const GLuint* textures) override;
  void DeleteBuffers(GLsizei n, const GLuint* buffers) override;
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

  GLuint CreateShader(GLenum type) override;
  GLuint CreateProgram() override;

  void BindTexture(GLenum target, GLuint texture) override;

  void GetIntegerv(GLenum pname, GLint* params) override;
  void GetShaderiv(GLuint shader, GLenum pname, GLint* params) override;
  void GetProgramiv(GLuint program, GLenum pname, GLint* params) override;
  void GetShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision) override;
  GLenum CheckFramebufferStatus(GLenum target) override;

  void UseProgram(GLuint program) override;
  void Flush() override;
  void Finish() override;
  void ShallowFinishCHROMIUM() override;

  void BindBuffer(GLenum target, GLuint buffer) override;
  void BindRenderbuffer(GLenum target, GLuint buffer) override;
  void BindFramebuffer(GLenum target, GLuint buffer) override;

  void PixelStorei(GLenum pname, GLint param) override;

  void* MapBufferCHROMIUM(GLuint target, GLenum access) override;
  GLboolean UnmapBufferCHROMIUM(GLuint target) override;
  void BufferData(GLenum target,
                  GLsizeiptr size,
                  const void* data,
                  GLenum usage) override;

  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

  GLuint CreateAndTexStorage2DSharedImageCHROMIUM(
      const GLbyte* mailbox) override;

  void ResizeCHROMIUM(GLuint width,
                      GLuint height,
                      float device_scale,
                      GLcolorSpace color_space,
                      GLboolean has_alpha) override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;
  GLenum GetGraphicsResetStatusKHR() override;

  void ReadPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  void* pixels) override;

  // Overridden from gpu::InterfaceBase
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  size_t NumTextures() const;

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return base::Contains(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

  size_t NumFramebuffers() const;
  size_t NumRenderbuffers() const;

  bool IsContextLost() { return context_lost_; }
  void set_test_support(TestContextSupport* test_support) {
    test_support_ = test_support;
  }
  const gpu::Capabilities& test_capabilities() const {
    return test_capabilities_;
  }
  const gpu::SyncToken& last_waited_sync_token() const {
    return last_waited_sync_token_;
  }
  void set_context_lost(bool context_lost) { context_lost_ = context_lost; }

  void set_support_texture_half_float_linear(bool support);
  void set_support_texture_norm16(bool support);
  void set_gpu_rasterization(bool gpu_rasterization);
  void set_max_texture_size(int size);
  void set_supports_gpu_memory_buffer_format(gfx::BufferFormat format,
                                             bool support);
  void set_supports_texture_rg(bool support);

  // When set, MapBufferCHROMIUM will return NULL after this many times.
  void set_times_map_buffer_chromium_succeeds(int times) {
    times_map_buffer_chromium_succeeds_ = times;
  }

  virtual GLuint NextTextureId();
  virtual void RetireTextureId(GLuint id);

  virtual GLuint NextBufferId();
  virtual void RetireBufferId(GLuint id);

  virtual GLuint NextImageId();
  virtual void RetireImageId(GLuint id);

  virtual GLuint NextFramebufferId();
  virtual void RetireFramebufferId(GLuint id);

  virtual GLuint NextRenderbufferId();
  virtual void RetireRenderbufferId(GLuint id);

  void set_context_lost_callback(base::OnceClosure callback) {
    context_lost_callback_ = std::move(callback);
  }

  int width() const { return width_; }
  int height() const { return height_; }
  bool reshape_called() const { return reshape_called_; }
  void clear_reshape_called() { reshape_called_ = false; }
  float scale_factor() const { return scale_factor_; }

  enum UpdateType { NO_UPDATE = 0, PREPARE_TEXTURE, POST_SUB_BUFFER };

  gfx::Rect update_rect() const { return update_rect_; }

  UpdateType last_update_type() { return last_update_type_; }

 protected:
  struct Buffer {
    Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    ~Buffer();

    GLenum target;
    std::unique_ptr<uint8_t[]> pixels;
    size_t size;
  };

  unsigned context_id_;
  gpu::Capabilities test_capabilities_;
  gpu::GLCapabilities test_gl_capabilities_;
  int times_end_query_succeeds_ = -1;
  bool context_lost_ = false;
  int times_map_buffer_chromium_succeeds_ = -1;
  base::OnceClosure context_lost_callback_;
  std::unordered_set<unsigned> used_textures_;
  unsigned next_program_id_ = 1000;
  std::unordered_set<unsigned> program_set_;
  unsigned next_shader_id_ = 2000;
  std::unordered_set<unsigned> shader_set_;
  unsigned next_framebuffer_id_ = 1;
  std::unordered_set<unsigned> framebuffer_set_;
  unsigned current_framebuffer_ = 0;
  std::vector<raw_ptr<TestGLES2Interface, VectorExperimental>> shared_contexts_;
  bool reshape_called_ = false;
  int width_ = 0;
  int height_ = 0;
  float scale_factor_ = -1.f;
  raw_ptr<TestContextSupport> test_support_ = nullptr;
  gfx::Rect update_rect_;
  UpdateType last_update_type_ = NO_UPDATE;
  GLuint64 next_insert_fence_sync_ = 1;
  gpu::SyncToken last_waited_sync_token_;
  int unpack_alignment_ = 4;

  base::flat_map<unsigned, unsigned> bound_buffer_;

  unsigned next_buffer_id_ = 1;
  unsigned next_image_id_ = 1;
  unsigned next_texture_id_ = 1;
  unsigned next_renderbuffer_id_ = 1;
  std::unordered_map<unsigned, std::unique_ptr<Buffer>> buffers_;
  std::unordered_set<unsigned> textures_;
  std::unordered_set<unsigned> renderbuffer_set_;

  base::WeakPtrFactory<TestGLES2Interface> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_GLES2_INTERFACE_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_COPIER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_COPIER_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gfx {
class ColorSpace;
class Rect;
class Vector2d;
}  // namespace gfx

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {

class ContextProvider;
class CopyOutputRequest;
class GLI420Converter;
class GLScaler;
class TextureDeleter;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// Helper class for GLRenderer that executes CopyOutputRequests using GL, to
// perform texture copies/transformations and read back bitmaps. Also manages
// the caching of resources needed to ensure efficient video performance.
//
// GLRenderer calls CopyFromTextureOrFramebuffer() to execute a
// CopyOutputRequest. GLRendererCopier will examine the request and determine
// the minimal amount of work needed to satisfy all the requirements of the
// request.
//
// In many cases, interim GL objects (textures, framebuffers, etc.) must be
// created as part of a multi-step process. When considering video performance
// (i.e., a series of CopyOutputRequests from the same "source"), these interim
// objects must be cached to prevent a significant performance penalty on some
// GPU/drivers. GLRendererCopier manages such a cache and automatically frees
// the objects when it detects that a stream of CopyOutputRequests from a given
// "source" has ended.
class VIZ_SERVICE_EXPORT GLRendererCopier {
 public:
  // Define types to avoid pulling in command buffer GL headers, which conflict
  // the ui/gl/gl_bindings.h
  using GLuint = unsigned int;
  using GLenum = unsigned int;

  // |texture_deleter| must outlive this instance.
  GLRendererCopier(scoped_refptr<ContextProvider> context_provider,
                   TextureDeleter* texture_deleter);

  ~GLRendererCopier();

  // Executes the |request|, copying from the currently-bound framebuffer of the
  // given |internal_format|. |output_rect| is the RenderPass's output Rect in
  // draw space, and is used to translate and clip the result selection Rect in
  // the request. |framebuffer_texture| and |framebuffer_texture_size| are
  // optional, but desired for performance: If provided, the texture might be
  // used as the source, to avoid having to make a copy of the framebuffer.
  // |flipped_source| is true (common case) if the framebuffer content is
  // vertically flipped (bottom-up row order). |color_space| specifies the color
  // space of the pixels in the framebuffer.
  //
  // This implementation may change a wide variety of GL state, such as texture
  // and framebuffer bindings, shader programs, and related attributes; and so
  // the caller must not make any assumptions about the state of the GL context
  // after this call.
  void CopyFromTextureOrFramebuffer(
      std::unique_ptr<CopyOutputRequest> request,
      const copy_output::RenderPassGeometry& geometry,
      GLenum internal_format,
      GLuint framebuffer_texture,
      const gfx::Size& framebuffer_texture_size,
      bool flipped_source,
      const gfx::ColorSpace& color_space);

  // Checks whether cached resources should be freed because recent copy
  // activity is no longer using them. This should be called after a frame has
  // finished drawing (after all copy requests have been executed).
  void FreeUnusedCachedResources();

  void set_async_gl_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    async_gl_task_runner_ = task_runner;
  }

 private:
  friend class GLRendererCopierTest;

  // The collection of resources that might be cached over multiple copy
  // requests from the same source. While executing a CopyOutputRequest, this
  // struct is also used to pass around intermediate objects between operations.
  struct VIZ_SERVICE_EXPORT ReusableThings {
    // This is used to determine whether these things aren't being used anymore.
    uint32_t purge_count_at_last_use = 0;

    // Texture containing a copy of the source framebuffer, if the source
    // framebuffer cannot be used directly.
    GLuint fb_copy_texture = 0;
    GLenum fb_copy_texture_internal_format = static_cast<GLenum>(0 /*GL_NONE*/);
    gfx::Size fb_copy_texture_size;

    // RGBA requests: Scaling, and texture/framebuffer for readback.
    std::unique_ptr<GLScaler> scaler;
    GLuint result_texture = 0;
    gfx::Size result_texture_size;
    GLuint readback_framebuffer = 0;

    // I420_PLANES requests: I420 scaling and format conversion, and
    // textures+framebuffers for readback.
    std::unique_ptr<GLI420Converter> i420_converter;
    std::array<GLuint, 3> yuv_textures = {0, 0, 0};
    gfx::Size y_texture_size;
    std::array<GLuint, 3> yuv_readback_framebuffers = {0, 0, 0};

    ReusableThings();
    ~ReusableThings();

    // Frees all the GL objects and scalers. This is in-lieu of a ReusableThings
    // destructor because a valid GL context is required to free some of the
    // objects.
    void Free(gpu::gles2::GLES2Interface* gl);

   private:
    DISALLOW_COPY_AND_ASSIGN(ReusableThings);
  };

  // Renders a scaled/transformed copy of a source texture according to the
  // |request| parameters and other source characteristics. |result_texture|
  // must be allocated/sized by the caller. For RGBA_BITMAP requests, the image
  // content will be rendered in top-down row order and maybe red-blue swapped,
  // to support efficient readback later on. For RGBA_TEXTURE requests, the
  // image content is always rendered Y-flipped (bottom-up row order).
  void RenderResultTexture(const CopyOutputRequest& request,
                           bool flipped_source,
                           const gfx::ColorSpace& source_color_space,
                           GLuint source_texture,
                           const gfx::Size& source_texture_size,
                           const gfx::Rect& sampling_rect,
                           const gfx::Rect& result_rect,
                           GLuint result_texture,
                           ReusableThings* things);

  // Similar to RenderResultTexture(), except also transform the image into I420
  // format (a popular video format). Three textures, representing each of the
  // Y/U/V planes (as described in GLI420Converter), are populated and their GL
  // references placed in |things|. The image content is always rendered in
  // top-down row order and swizzled (if needed), to support efficient readback
  // later on.
  //
  // For alignment reasons, sometimes a slightly larger result will be provided,
  // and the return Rect will indicate the actual bounds that were rendered
  // (|result_rect|'s coordinate system). See StartI420ReadbackFromTextures()
  // for more details.
  gfx::Rect RenderI420Textures(const CopyOutputRequest& request,
                               bool flipped_source,
                               const gfx::ColorSpace& source_color_space,
                               GLuint source_texture,
                               const gfx::Size& source_texture_size,
                               const gfx::Rect& sampling_rect,
                               const gfx::Rect& result_rect,
                               ReusableThings* things);

  // Binds the |things->result_texture| to a framebuffer and calls
  // StartReadbackFromFramebuffer(). This is for RGBA_BITMAP requests only.
  // Assumes the image content is in top-down row order (and is red-blue swapped
  // iff RenderResultTexture() would have done that).
  void StartReadbackFromTexture(std::unique_ptr<CopyOutputRequest> request,
                                const gfx::Rect& result_rect,
                                const gfx::ColorSpace& color_space,
                                ReusableThings* things);

  // Processes the next phase of the copy request by starting readback from the
  // currently-bound framebuffer into a pixel transfer buffer. |readback_offset|
  // is the origin of the readback rect within the framebuffer, with
  // |result_rect| providing the size of the readback rect. |flipped_source| is
  // true if the framebuffer content is in bottom-up row order, and
  // |swapped_red_and_blue| specifies whether the red and blue channels have
  // been swapped. This method kicks-off an asynchronous glReadPixels()
  // workflow.
  void StartReadbackFromFramebuffer(std::unique_ptr<CopyOutputRequest> request,
                                    const gfx::Vector2d& readback_offset,
                                    bool flipped_source,
                                    bool swapped_red_and_blue,
                                    const gfx::Rect& result_rect,
                                    const gfx::ColorSpace& color_space);

  // Renders a scaled/transformed copy of a source texture similarly to
  // RenderResultTexture, but packages up the result in a mailbox and sends it
  // as the result to the CopyOutputRequest.
  void RenderAndSendTextureResult(std::unique_ptr<CopyOutputRequest> request,
                                  bool flipped_source,
                                  const gfx::ColorSpace& color_space,
                                  GLuint source_texture,
                                  const gfx::Size& source_texture_size,
                                  const gfx::Rect& sampling_rect,
                                  const gfx::Rect& result_rect,
                                  ReusableThings* things);

  // Like StartReadbackFromTexture(), except that this processes the three Y/U/V
  // result textures in |things| by using three framebuffers and three
  // asynchronous readback operations. A single pixel transfer buffer is used to
  // hold the results of all three readbacks (i.e., each plane starts at a
  // different offset in the transfer buffer).
  //
  // |aligned_rect| is the Rect returned from the RenderI420Textures() call, and
  // is required so that the CopyOutputResult sent at the end of this workflow
  // will access the correct region of pixels.
  void StartI420ReadbackFromTextures(std::unique_ptr<CopyOutputRequest> request,
                                     const gfx::Rect& aligned_rect,
                                     const gfx::Rect& result_rect,
                                     ReusableThings* things);

  // Retrieves a cached ReusableThings instance for the given CopyOutputRequest
  // source, or creates a new instance.
  std::unique_ptr<ReusableThings> TakeReusableThingsOrCreate(
      const base::UnguessableToken& requester);

  // If |requester| is a valid UnguessableToken, this stashes the given
  // ReusableThings instance in the cache for use in future CopyOutputRequests
  // from the same requester. Otherwise, |things| is freed.
  void StashReusableThingsOrDelete(const base::UnguessableToken& requester,
                                   std::unique_ptr<ReusableThings> things);

  // Queries the GL implementation to determine which is the more performance-
  // optimal supported readback format: GL_RGBA or GL_BGRA_EXT, and memoizes the
  // result for all future calls.
  //
  // Precondition: The GL context has a complete, bound framebuffer ready for
  // readback.
  GLenum GetOptimalReadbackFormat();

  // Returns true if the red and blue channels should be swapped within the GPU,
  // where such an operation has negligible cost, so that later the red-blue
  // swap does not need to happen on the CPU (non-negligible cost).
  bool ShouldSwapRedAndBlueForBitmapReadback();

  // Injected dependencies.
  const scoped_refptr<ContextProvider> context_provider_;
  TextureDeleter* const texture_deleter_;

  // This increments by one for every call to FreeUnusedCachedResources(). It
  // is meant to determine when cached resources should be freed because they
  // are unlikely to see further use.
  uint32_t purge_counter_ = 0;

  // A cache of resources recently used in the execution of a stream of copy
  // requests from the same source. Since this reflects the number of active
  // video captures, it is expected to almost always be zero or one entry in
  // size.
  base::flat_map<base::UnguessableToken, std::unique_ptr<ReusableThings>>
      cache_;

  // This specifies whether the GPU+driver combination executes readback more
  // efficiently using GL_RGBA or GL_BGRA_EXT format. This starts out as
  // GL_NONE, which means "unknown," and will be determined at the time the
  // first readback request is made.
  GLenum optimal_readback_format_ = static_cast<GLenum>(0 /*GL_NONE*/);

  // Purge cache entries that have not been used after this many calls to
  // FreeUnusedCachedResources(). The choice of 60 is arbitrary, but on most
  // platforms means that a somewhat-to-fully active compositor will cause
  // things to be auto-purged after approx. 1-2 seconds of not being used.
  static constexpr int kKeepalivePeriod = 60;

  // The task runner that is being used to call into GLRendererCopier. This
  // allows for CopyOutputResults, owned by external entities, to execute
  // post-destruction clean-up tasks. If null, assume CopyOutputResults are
  // always destroyed from the same task runner
  scoped_refptr<base::SingleThreadTaskRunner> async_gl_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GLRendererCopier);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_COPIER_H_

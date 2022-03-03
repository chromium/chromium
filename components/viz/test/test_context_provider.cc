// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_context_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/config/skia_limits.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {

namespace {

// Various tests rely on functionality (capabilities) enabled by these extension
// strings.
const char* const kExtensions[] = {"GL_EXT_stencil_wrap",
                                   "GL_EXT_texture_format_BGRA8888",
                                   "GL_OES_rgb8_rgba8",
                                   "GL_EXT_texture_norm16",
                                   "GL_CHROMIUM_framebuffer_multisample",
                                   "GL_CHROMIUM_renderbuffer_format_BGRA8888",
                                   "GL_OES_texture_half_float",
                                   "GL_OES_texture_half_float_linear",
                                   "GL_EXT_color_buffer_half_float"};

// A test RasterInterface implementation that doesn't crash in
// BeginRasterCHROMIUM(), RasterCHROMIUM() and EndRasterCHROMIUM().
// TODO(crbug.com/1164071): This RasterInterface implementation should be
// replaced by something (that doesn't exist yet) like TestRasterInterface that
// isn't based on RasterImplementationGLES.
class RasterImplementationForOOPR
    : public gpu::raster::RasterImplementationGLES {
 public:
  explicit RasterImplementationForOOPR(gpu::gles2::GLES2Interface* gl,
                                       gpu::ContextSupport* support)
      : gpu::raster::RasterImplementationGLES(gl, support) {}
  ~RasterImplementationForOOPR() override = default;

  // gpu::raster::RasterInterface implementation.
  void GetQueryObjectui64vEXT(GLuint id,
                              GLenum pname,
                              GLuint64* params) override {
    // This is used for testing GL_COMMANDS_ISSUED_TIMESTAMP_QUERY, so we return
    // the maximum that base::TimeDelta()::InMicroseconds() could return.
    if (pname == GL_QUERY_RESULT_EXT) {
      static_assert(std::is_same<decltype(base::TimeDelta().InMicroseconds()),
                                 int64_t>::value,
                    "Expected the return type of "
                    "base::TimeDelta()::InMicroseconds() to be int64_t");
      *params = std::numeric_limits<int64_t>::max();
    } else {
      NOTREACHED();
    }
  }
  void BeginRasterCHROMIUM(GLuint sk_color,
                           GLboolean needs_clear,
                           GLuint msaa_sample_count,
                           gpu::raster::MsaaMode msaa_mode,
                           GLboolean can_use_lcd_text,
                           GLboolean visible,
                           const gfx::ColorSpace& color_space,
                           const GLbyte* mailbox) override {}
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      const gfx::Vector2dF& post_scale,
                      bool requires_clear,
                      size_t* max_op_size_hint,
                      bool preserve_recording = true) override {}
  void EndRasterCHROMIUM() override {}
};

class TestGLES2InterfaceForContextProvider : public TestGLES2Interface {
 public:
  TestGLES2InterfaceForContextProvider(
      std::string additional_extensions = std::string())
      : extension_string_(
            BuildExtensionString(std::move(additional_extensions))) {}

  TestGLES2InterfaceForContextProvider(
      const TestGLES2InterfaceForContextProvider&) = delete;
  TestGLES2InterfaceForContextProvider& operator=(
      const TestGLES2InterfaceForContextProvider&) = delete;

  ~TestGLES2InterfaceForContextProvider() override = default;

  // TestGLES2Interface:
  const GLubyte* GetString(GLenum name) override {
    switch (name) {
      case GL_EXTENSIONS:
        return reinterpret_cast<const GLubyte*>(extension_string_.c_str());
      case GL_VERSION:
        return reinterpret_cast<const GrGLubyte*>("4.0 Null GL");
      case GL_SHADING_LANGUAGE_VERSION:
        return reinterpret_cast<const GrGLubyte*>("4.20.8 Null GLSL");
      case GL_VENDOR:
        return reinterpret_cast<const GrGLubyte*>("Null Vendor");
      case GL_RENDERER:
        return reinterpret_cast<const GrGLubyte*>("The Null (Non-)Renderer");
    }
    return nullptr;
  }
  const GrGLubyte* GetStringi(GrGLenum name, GrGLuint i) override {
    if (name == GL_EXTENSIONS && i < std::size(kExtensions))
      return reinterpret_cast<const GLubyte*>(kExtensions[i]);
    return nullptr;
  }
  void GetIntegerv(GLenum name, GLint* params) override {
    switch (name) {
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        *params = 8;
        return;
      case GL_MAX_RENDERBUFFER_SIZE:
        *params = 2048;
        break;
      case GL_MAX_TEXTURE_SIZE:
        *params = 2048;
        break;
      case GL_MAX_TEXTURE_IMAGE_UNITS:
        *params = 8;
        break;
      case GL_MAX_VERTEX_ATTRIBS:
        *params = 8;
        break;
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        *params = 0;
        return;
      default:
        break;
    }
    TestGLES2Interface::GetIntegerv(name, params);
  }

 private:
  static std::string BuildExtensionString(std::string additional_extensions) {
    std::string extension_string = kExtensions[0];
    for (size_t i = 1; i < std::size(kExtensions); ++i) {
      extension_string += " ";
      extension_string += kExtensions[i];
    }
    if (!additional_extensions.empty()) {
      extension_string += " ";
      extension_string += additional_extensions;
    }
    return extension_string;
  }

  const std::string extension_string_;
};

}  // namespace

TestSharedImageInterface::TestSharedImageInterface() = default;
TestSharedImageInterface::~TestSharedImageInterface() = default;

gpu::Mailbox TestSharedImageInterface::CreateSharedImage(
    ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    gpu::SurfaceHandle surface_handle) {
  base::AutoLock locked(lock_);
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  shared_images_.insert(mailbox);
  most_recent_size_ = size;
  return mailbox;
}

gpu::Mailbox TestSharedImageInterface::CreateSharedImage(
    ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  base::AutoLock locked(lock_);
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  shared_images_.insert(mailbox);
  return mailbox;
}

gpu::Mailbox TestSharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  base::AutoLock locked(lock_);
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  shared_images_.insert(mailbox);
  most_recent_size_ = gpu_memory_buffer->GetSize();
  return mailbox;
}

std::vector<gpu::Mailbox>
TestSharedImageInterface::CreateSharedImageVideoPlanes(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    uint32_t usage) {
  base::AutoLock locked(lock_);
  auto mailboxes =
      std::vector<gpu::Mailbox>{gpu::Mailbox::GenerateForSharedImage(),
                                gpu::Mailbox::GenerateForSharedImage()};
  shared_images_.insert(mailboxes[0]);
  shared_images_.insert(mailboxes[1]);
  most_recent_size_ = gpu_memory_buffer->GetSize();
  return mailboxes;
}

gpu::Mailbox TestSharedImageInterface::CreateSharedImageWithAHB(
    const gpu::Mailbox& mailbox,
    uint32_t usage,
    const gpu::SyncToken& sync_token) {
  base::AutoLock locked(lock_);
  auto out_mailbox = gpu::Mailbox::GenerateForSharedImage();
  shared_images_.insert(out_mailbox);
  return out_mailbox;
}

void TestSharedImageInterface::UpdateSharedImage(
    const gpu::SyncToken& sync_token,
    const gpu::Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  DCHECK(shared_images_.find(mailbox) != shared_images_.end());
}

void TestSharedImageInterface::UpdateSharedImage(
    const gpu::SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const gpu::Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  DCHECK(shared_images_.find(mailbox) != shared_images_.end());
}

void TestSharedImageInterface::DestroySharedImage(
    const gpu::SyncToken& sync_token,
    const gpu::Mailbox& mailbox) {
  base::AutoLock locked(lock_);
  shared_images_.erase(mailbox);
  most_recent_destroy_token_ = sync_token;
}

gpu::SharedImageInterface::SwapChainMailboxes
TestSharedImageInterface::CreateSwapChain(ResourceFormat format,
                                          const gfx::Size& size,
                                          const gfx::ColorSpace& color_space,
                                          GrSurfaceOrigin surface_origin,
                                          SkAlphaType alpha_type,
                                          uint32_t usage) {
  auto front_buffer = gpu::Mailbox::GenerateForSharedImage();
  auto back_buffer = gpu::Mailbox::GenerateForSharedImage();
  shared_images_.insert(front_buffer);
  shared_images_.insert(back_buffer);
  return {front_buffer, back_buffer};
}

void TestSharedImageInterface::PresentSwapChain(
    const gpu::SyncToken& sync_token,
    const gpu::Mailbox& mailbox) {}

#if BUILDFLAG(IS_FUCHSIA)
void TestSharedImageInterface::RegisterSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  NOTREACHED();
}

void TestSharedImageInterface::ReleaseSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id) {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_FUCHSIA)

gpu::SyncToken TestSharedImageInterface::GenVerifiedSyncToken() {
  base::AutoLock locked(lock_);
  most_recent_generated_token_ =
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId(), ++release_id_);
  most_recent_generated_token_.SetVerifyFlush();
  return most_recent_generated_token_;
}

gpu::SyncToken TestSharedImageInterface::GenUnverifiedSyncToken() {
  base::AutoLock locked(lock_);
  most_recent_generated_token_ =
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId(), ++release_id_);
  return most_recent_generated_token_;
}

void TestSharedImageInterface::WaitSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED();
}

void TestSharedImageInterface::Flush() {
  // No need to flush in this implementation.
}

scoped_refptr<gfx::NativePixmap> TestSharedImageInterface::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  return nullptr;
}

bool TestSharedImageInterface::CheckSharedImageExists(
    const gpu::Mailbox& mailbox) const {
  base::AutoLock locked(lock_);
  return shared_images_.contains(mailbox);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::string additional_extensions) {
  constexpr bool support_locking = false;
  return new TestContextProvider(
      std::make_unique<TestContextSupport>(),
      std::make_unique<TestGLES2InterfaceForContextProvider>(
          std::move(additional_extensions)),
      /*sii=*/nullptr, support_locking);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateWorker() {
  return CreateWorker(std::make_unique<TestContextSupport>());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateWorker(
    std::unique_ptr<TestContextSupport> support) {
  DCHECK(support);
  std::unique_ptr<TestGLES2Interface> gles2 =
      std::make_unique<TestGLES2InterfaceForContextProvider>();

  // Worker contexts in browser/renderer will always use OOP-R for raster when
  // GPU accelerated raster is allowed. Main thread contexts typically don't
  // support OOP-R except for renderer main when OOP canvas is enabled.
  gles2->set_supports_oop_raster(true);
  std::unique_ptr<gpu::raster::RasterInterface> raster =
      std::make_unique<RasterImplementationForOOPR>(gles2.get(), support.get());

  auto worker_context_provider = base::MakeRefCounted<TestContextProvider>(
      std::move(support), std::move(gles2), std::move(raster),
      /*sii=*/nullptr, /*support_locking=*/true);

  // Worker contexts are bound to the thread they are created on.
  auto result = worker_context_provider->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    return nullptr;
  return worker_context_provider;
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestGLES2Interface> gl) {
  DCHECK(gl);
  constexpr bool support_locking = false;
  return new TestContextProvider(std::make_unique<TestContextSupport>(),
                                 std::move(gl), /*sii=*/nullptr,
                                 support_locking);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestSharedImageInterface> sii) {
  DCHECK(sii);
  constexpr bool support_locking = false;
  return new TestContextProvider(std::make_unique<TestContextSupport>(),
                                 /*gl=*/nullptr, std::move(sii),
                                 support_locking);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::unique_ptr<TestContextSupport> support) {
  DCHECK(support);
  constexpr bool support_locking = false;
  return new TestContextProvider(
      std::move(support),
      std::make_unique<TestGLES2InterfaceForContextProvider>(),
      /*sii=*/nullptr, support_locking);
}

TestContextProvider::TestContextProvider(
    std::unique_ptr<TestContextSupport> support,
    std::unique_ptr<TestGLES2Interface> gl,
    std::unique_ptr<TestSharedImageInterface> sii,
    bool support_locking)
    : TestContextProvider(std::move(support),
                          std::move(gl),
                          /*raster=*/nullptr,
                          std::move(sii),
                          support_locking) {}

TestContextProvider::TestContextProvider(
    std::unique_ptr<TestContextSupport> support,
    std::unique_ptr<TestGLES2Interface> gl,
    std::unique_ptr<gpu::raster::RasterInterface> raster,
    std::unique_ptr<TestSharedImageInterface> sii,
    bool support_locking)
    : support_(std::move(support)),
      context_gl_(
          gl ? std::move(gl)
             : std::make_unique<TestGLES2InterfaceForContextProvider>()),
      raster_context_(std::move(raster)),
      shared_image_interface_(
          sii ? std::move(sii) : std::make_unique<TestSharedImageInterface>()),
      support_locking_(support_locking) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context_gl_);
  context_thread_checker_.DetachFromThread();
  context_gl_->set_test_support(support_.get());
  if (!raster_context_) {
    raster_context_ = std::make_unique<gpu::raster::RasterImplementationGLES>(
        context_gl_.get(), support_.get());
  }
  // Just pass nullptr to the ContextCacheController for its task runner.
  // Idle handling is tested directly in ContextCacheController's
  // unittests, and isn't needed here.
  cache_controller_ =
      std::make_unique<ContextCacheController>(support_.get(), nullptr);
}

TestContextProvider::~TestContextProvider() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());
}

void TestContextProvider::AddRef() const {
  base::RefCountedThreadSafe<TestContextProvider>::AddRef();
}

void TestContextProvider::Release() const {
  base::RefCountedThreadSafe<TestContextProvider>::Release();
}

gpu::ContextResult TestContextProvider::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (!bound_) {
    if (context_gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
      return gpu::ContextResult::kTransientFailure;

    context_gl_->set_context_lost_callback(base::BindOnce(
        &TestContextProvider::OnLostContext, base::Unretained(this)));
  }
  bound_ = true;
  return gpu::ContextResult::kSuccess;
}

const gpu::Capabilities& TestContextProvider::ContextCapabilities() const {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();
  return context_gl_->test_capabilities();
}

const gpu::GpuFeatureInfo& TestContextProvider::GetGpuFeatureInfo() const {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();
  return gpu_feature_info_;
}

gpu::gles2::GLES2Interface* TestContextProvider::ContextGL() {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();

  return context_gl_.get();
}

gpu::raster::RasterInterface* TestContextProvider::RasterInterface() {
  return raster_context_.get();
}

gpu::ContextSupport* TestContextProvider::ContextSupport() {
  return support();
}

class GrDirectContext* TestContextProvider::GrContext() {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();

  if (gr_context_)
    return gr_context_->get();

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::DefaultGrCacheLimitsForTests(&max_resource_cache_bytes,
                                    &max_glyph_cache_texture_bytes);
  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      context_gl_.get(), support_.get(), context_gl_->test_capabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes, true);
  cache_controller_->SetGrContext(gr_context_->get());

  // If GlContext is already lost, also abandon the new GrContext.
  if (ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

TestSharedImageInterface* TestContextProvider::SharedImageInterface() {
  return shared_image_interface_.get();
}

ContextCacheController* TestContextProvider::CacheController() {
  CheckValidThreadOrLockAcquired();
  return cache_controller_.get();
}

base::Lock* TestContextProvider::GetLock() {
  if (!support_locking_)
    return nullptr;
  return &context_lock_;
}

void TestContextProvider::OnLostContext() {
  CheckValidThreadOrLockAcquired();
  for (auto& observer : observers_)
    observer.OnContextLost();
  if (gr_context_)
    gr_context_->get()->abandonContext();
}

TestGLES2Interface* TestContextProvider::TestContextGL() {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();
  return context_gl_.get();
}

void TestContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void TestContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

}  // namespace viz

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/test/test_context_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/config/skia_limits.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"

namespace viz {

namespace {

// Various tests rely on functionality (capabilities) enabled by these extension
// strings.
const char* const kExtensions[] = {"GL_ARB_texture_rectangle",
                                   "GL_EXT_stencil_wrap",
                                   "GL_EXT_texture_format_BGRA8888",
                                   "GL_OES_rgb8_rgba8",
                                   "GL_EXT_texture_norm16",
                                   "GL_CHROMIUM_framebuffer_multisample",
                                   "GL_CHROMIUM_renderbuffer_format_BGRA8888",
                                   "GL_OES_texture_half_float",
                                   "GL_OES_texture_half_float_linear",
                                   "GL_EXT_color_buffer_half_float"};

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

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    std::string additional_extensions) {
  constexpr bool support_locking = false;
  return new TestContextProvider(
      std::make_unique<TestContextSupport>(),
      std::make_unique<TestGLES2InterfaceForContextProvider>(
          std::move(additional_extensions)),
      /*raster=*/nullptr,
      /*sii=*/nullptr, support_locking);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateRaster() {
  return CreateRaster(std::make_unique<TestContextSupport>());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateRaster(
    std::unique_ptr<TestRasterInterface> raster) {
  CHECK(raster);
  return base::MakeRefCounted<TestContextProvider>(
      std::make_unique<TestContextSupport>(), std::move(raster),
      /*support_locking=*/false);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateRaster(
    std::unique_ptr<TestContextSupport> context_support) {
  CHECK(context_support);
  return base::MakeRefCounted<TestContextProvider>(
      std::move(context_support), std::make_unique<TestRasterInterface>(),
      /*support_locking=*/false);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateWorker() {
  return CreateWorker(std::make_unique<TestContextSupport>());
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::CreateWorker(
    std::unique_ptr<TestContextSupport> support) {
  DCHECK(support);

  auto worker_context_provider = base::MakeRefCounted<TestContextProvider>(
      std::move(support), std::make_unique<TestRasterInterface>(),
      /*support_locking=*/true);

  // Worker contexts are bound to the thread they are created on.
  auto result = worker_context_provider->BindToCurrentSequence();
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
                                 std::move(gl), /*raster=*/nullptr,
                                 /*sii=*/nullptr, support_locking);
}

// static
scoped_refptr<TestContextProvider> TestContextProvider::Create(
    scoped_refptr<gpu::TestSharedImageInterface> sii) {
  DCHECK(sii);
  constexpr bool support_locking = false;
  return new TestContextProvider(
      std::make_unique<TestContextSupport>(),
      std::make_unique<TestGLES2InterfaceForContextProvider>(),
      /*raster=*/nullptr, std::move(sii), support_locking);
}

TestContextProvider::TestContextProvider(
    std::unique_ptr<TestContextSupport> support,
    std::unique_ptr<TestRasterInterface> raster,
    bool support_locking)
    : support_(std::move(support)),
      raster_context_(std::move(raster)),
      shared_image_interface_(
          base::MakeRefCounted<gpu::TestSharedImageInterface>()),
      support_locking_(support_locking) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(raster_context_);

  context_thread_checker_.DetachFromThread();
  raster_context_->set_test_support(support_.get());

  // Just pass nullptr to the ContextCacheController for its task runner.
  // Idle handling is tested directly in ContextCacheController's
  // unittests, and isn't needed here.
  cache_controller_ =
      std::make_unique<ContextCacheController>(support_.get(), nullptr);
}

TestContextProvider::TestContextProvider(
    std::unique_ptr<TestContextSupport> support,
    std::unique_ptr<TestGLES2Interface> gl,
    std::unique_ptr<gpu::raster::RasterInterface> raster,
    scoped_refptr<gpu::TestSharedImageInterface> sii,
    bool support_locking)
    : support_(std::move(support)),
      context_gl_(std::move(gl)),
      raster_interface_gles_(std::move(raster)),
      support_locking_(support_locking) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context_gl_);
  context_thread_checker_.DetachFromThread();
  context_gl_->set_test_support(support_.get());
  if (!raster_interface_gles_) {
    raster_interface_gles_ =
        std::make_unique<gpu::raster::RasterImplementationGLES>(
            context_gl_.get(), support_.get(),
            context_gl_->test_capabilities());
  }
  // Just pass nullptr to the ContextCacheController for its task runner.
  // Idle handling is tested directly in ContextCacheController's
  // unittests, and isn't needed here.
  cache_controller_ =
      std::make_unique<ContextCacheController>(support_.get(), nullptr);

  if (sii) {
    shared_image_interface_ = std::move(sii);
  } else {
    shared_image_interface_ =
        base::MakeRefCounted<gpu::TestSharedImageInterface>();

    // By default, luminance textures are supported in GLES2.
    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_luminance_shared_images = true;

    shared_image_interface_->SetCapabilities(shared_image_caps);
  }
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

gpu::ContextResult TestContextProvider::BindToCurrentSequence() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (!bound_) {
    if (context_gl_) {
      if (context_gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
        return gpu::ContextResult::kTransientFailure;

      context_gl_->set_context_lost_callback(base::BindOnce(
          &TestContextProvider::OnLostContext, base::Unretained(this)));
    } else {
      if (raster_context_->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
        return gpu::ContextResult::kTransientFailure;

      raster_context_->set_context_lost_callback(base::BindOnce(
          &TestContextProvider::OnLostContext, base::Unretained(this)));
    }
  }
  bound_ = true;
  return gpu::ContextResult::kSuccess;
}

const gpu::Capabilities& TestContextProvider::ContextCapabilities() const {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();
  if (context_gl_)
    return context_gl_->test_capabilities();

  return raster_context_->capabilities();
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
  return raster_context_ ? raster_context_.get() : raster_interface_gles_.get();
}

gpu::ContextSupport* TestContextProvider::ContextSupport() {
  return support();
}

class GrDirectContext* TestContextProvider::GrContext() {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();

  if (!context_gl_)
    return nullptr;

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

gpu::TestSharedImageInterface* TestContextProvider::SharedImageInterface() {
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

TestRasterInterface* TestContextProvider::GetTestRasterInterface() {
  DCHECK(bound_);
  CheckValidThreadOrLockAcquired();
  return raster_context_.get();
}

TestRasterInterface* TestContextProvider::UnboundTestRasterInterface() {
  return raster_context_.get();
}

void TestContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void TestContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

unsigned int TestContextProvider::GetGrGLTextureFormat(
    SharedImageFormat format) const {
  return SharedImageFormatRestrictedSinglePlaneUtils::ToGLTextureStorageFormat(
      format, ContextCapabilities().angle_rgbx_internal_format);
}

}  // namespace viz

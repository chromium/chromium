// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_RASTER_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_RASTER_CONTEXT_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"

class GrContext;

namespace base {
class Lock;
}

namespace gpu {
class ContextSupport;
struct GpuFeatureInfo;
class SharedImageInterface;

namespace gles2 {
class GLES2Interface;
}

namespace raster {
class RasterInterface;
}
}  // namespace gpu

namespace viz {

class VIZ_COMMON_EXPORT RasterContextProvider {
 public:
  class VIZ_COMMON_EXPORT ScopedRasterContextLock {
   public:
    explicit ScopedRasterContextLock(RasterContextProvider* context_provider,
                                     const char* url = nullptr);
    ~ScopedRasterContextLock();

    gpu::raster::RasterInterface* RasterInterface() {
      return context_provider_->RasterInterface();
    }

   private:
    RasterContextProvider* const context_provider_;
    base::AutoLock context_lock_;
    std::unique_ptr<ContextCacheController::ScopedBusy> busy_;
  };

  // RefCounted interface.
  virtual void AddRef() const = 0;
  virtual void Release() const = 0;

  // Bind the 3d context to the current thread. This should be called before
  // accessing the contexts. Calling it more than once should have no effect.
  // Once this function has been called, the class should only be accessed
  // from the same thread unless the function has some explicitly specified
  // rules for access on a different thread. See SetupLockOnMainThread(), which
  // can be used to provide access from multiple threads.
  virtual gpu::ContextResult BindToCurrentThread() = 0;

  // Adds/removes an observer to be called when the context is lost. AddObserver
  // should be called before BindToCurrentThread from the same thread that the
  // context is bound to, or any time while the lock is acquired after checking
  // for context loss.
  // NOTE: Implementations must avoid post-tasking the to the observer directly
  // as the observer may remove itself before the task runs.
  virtual void AddObserver(ContextLostObserver* obs) = 0;
  virtual void RemoveObserver(ContextLostObserver* obs) = 0;

  // Returns the lock that should be held if using this context from multiple
  // threads. This can be called on any thread.
  // Returns null if the context does not support locking and must be used from
  // the same thread.
  // NOTE: Helper method for ScopedContextLock. Use that instead of calling this
  // directly.
  virtual base::Lock* GetLock() = 0;

  // Get a CacheController interface to the 3d context.  The context provider
  // must have been successfully bound to a thread before calling this.
  virtual ContextCacheController* CacheController() = 0;

  // Get a ContextSupport interface to the 3d context.  The context provider
  // must have been successfully bound to a thread before calling this.
  virtual gpu::ContextSupport* ContextSupport() = 0;

  // Get a Skia GPU raster interface to the 3d context.  The context provider
  // must have been successfully bound to a thread before calling this.  Returns
  // nullptr if a GrContext fails to initialize on this context.
  virtual class GrContext* GrContext() = 0;

  virtual gpu::SharedImageInterface* SharedImageInterface() = 0;

  // Returns the capabilities of the currently bound 3d context.  The context
  // provider must have been successfully bound to a thread before calling this.
  virtual const gpu::Capabilities& ContextCapabilities() const = 0;

  // Returns feature blacklist decisions and driver bug workarounds info.  The
  // context provider must have been successfully bound to a thread before
  // calling this.
  virtual const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const = 0;

  // TODO(vmiura): Hide ContextGL() & GrContext() behind some kind of lock.

  // Get a GLES2 interface to the 3d context.  The context provider must have
  // been successfully bound to a thread before calling this.
  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;

  // Get a Raster interface to the 3d context.  The context provider must have
  // been successfully bound to a thread before calling this.
  virtual gpu::raster::RasterInterface* RasterInterface() = 0;

 protected:
  virtual ~RasterContextProvider() = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_RASTER_CONTEXT_PROVIDER_H_

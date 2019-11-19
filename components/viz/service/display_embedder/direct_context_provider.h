// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DIRECT_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DIRECT_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
class GrContext;

namespace gpu {
class CommandBufferDirect;
class DecoderContext;
class SharedImageInterface;
class TransferBuffer;
struct GpuPreferences;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2Interface;
class TextureManager;
}  // namespace gles2
}  // namespace gpu

namespace viz {
class ContextLostObserver;

// DirectContextProvider bridges between GPU client code and GPU service
// code. Often we cannot include client and service headers in the same
// translation unit due to collisions between client and service GL
// defines. DirectContextProviderDelegate satisfies DirectContextProvider
// dependencies that require GPU service code that cannot be included by
// DirectContextProvider.
class VIZ_SERVICE_EXPORT DirectContextProviderDelegate {
 public:
  DirectContextProviderDelegate();
  virtual ~DirectContextProviderDelegate();

  virtual gpu::SharedImageManager* GetSharedImageManager() = 0;
  virtual gpu::SharedImageInterface* GetSharedImageInterface() = 0;

  virtual gpu::CommandBufferNamespace GetNamespaceID() const = 0;
  virtual gpu::CommandBufferId GetCommandBufferID() const = 0;

  // Generate the release ID for a new SyncToken (see
  // GpuControl::GenerateFenceSyncRelease).
  virtual uint64_t GenerateFenceSyncRelease() = 0;

  // Runs |callback| when |sync_token| is released.
  virtual void SignalSyncToken(const gpu::SyncToken& sync_token,
                               base::OnceClosure callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectContextProviderDelegate);
};

// DirectContextProvider provides a GLES2Interface by running cross process code
// (e.g. GLES2Implementation and GLES2Decoder) within a single thread. It is
// suitable for easily porting code relying on GLES2Interface, but is less
// efficient than calling native GL because it serializes/deserializes command
// streams, validates command streams, and has unnecessary copies through shared
// memory (e.g. glReadPixels frame buffer). Parts of GLES2Interface are
// NOTIMPLEMENTED().
class VIZ_SERVICE_EXPORT DirectContextProvider
    : public base::RefCountedThreadSafe<DirectContextProvider>,
      public ContextProvider,
      public gpu::GpuControl,
      public base::trace_event::MemoryDumpProvider {
 public:
  DirectContextProvider(scoped_refptr<gl::GLContext> gl_context,
                        scoped_refptr<gl::GLSurface> gl_surface,
                        bool supports_alpha,
                        const gpu::GpuPreferences& gpu_preferences,
                        gpu::gles2::FeatureInfo* feature_info,
                        std::unique_ptr<DirectContextProviderDelegate> client);
  gpu::DecoderContext* decoder() { return decoder_.get(); }

  // Set required state, including texture_client_id as color attachment 0
  // of a currently bound framebuffer. If texture_client_id == 0, set FBO0 as
  // current.
  void SetGLRendererCopierRequiredState(GLuint texture_client_id);
  gpu::gles2::TextureManager* texture_manager();
  GLuint GenClientTextureId();
  void DeleteClientTextureId(GLuint client_id);
  void MarkContextLost();
  // Call a glFinish() to complete any pending queries.
  void FinishQueries();

  // ContextProvider implementation.
  void AddRef() const override;
  void Release() const override;
  gpu::ContextResult BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  ContextCacheController* CacheController() override;
  base::Lock* GetLock() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override;
  void AddObserver(ContextLostObserver* obs) override;
  void RemoveObserver(ContextLostObserver* obs) override;

  // GpuControl implementation.
  void SetGpuControlClient(gpu::GpuControlClient*) override;
  const gpu::Capabilities& GetCapabilities() const override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height) override;
  void DestroyImage(int32_t id) override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  void FlushPendingWork() override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncReleased(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) override;
  void SetDisplayTransform(gfx::OverlayTransform transform) override;

 private:
  friend class base::RefCountedThreadSafe<DirectContextProvider>;
  ~DirectContextProvider() override;

  void Destroy();
  void OnContextLost();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  gpu::gles2::MailboxManagerImpl mailbox_manager_;
  gpu::gles2::TraceOutputter outputter_;
  gpu::gles2::ImageManager image_manager_;
  gpu::ServiceDiscardableManager discardable_manager_;
  gpu::PassthroughDiscardableManager passthrough_discardable_manager_;
  gpu::gles2::ShaderTranslatorCache translator_cache_;
  gpu::gles2::FramebufferCompletenessCache completeness_cache_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  gpu::Capabilities capabilities_;
  gpu::ContextResult context_result_ = gpu::ContextResult::kSuccess;
  std::unique_ptr<DirectContextProviderDelegate> delegate_;

  // Only non-null if BindToCurrentThread() == ContextResult::kSuccess.
  std::unique_ptr<gpu::CommandBufferDirect> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_cmd_helper_;
  std::unique_ptr<gpu::gles2::GLES2Decoder> decoder_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;

  GLuint framebuffer_id_ = 0;

  base::ObserverList<ContextLostObserver>::Unchecked observers_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_DIRECT_CONTEXT_PROVIDER_H_

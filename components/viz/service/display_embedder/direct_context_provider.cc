// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/direct_context_provider.h"

#include <stdint.h>

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace viz {

DirectContextProviderDelegate::DirectContextProviderDelegate() = default;
DirectContextProviderDelegate::~DirectContextProviderDelegate() = default;

DirectContextProvider::DirectContextProvider(
    scoped_refptr<gl::GLContext> gl_context,
    scoped_refptr<gl::GLSurface> gl_surface,
    bool supports_alpha,
    const gpu::GpuPreferences& gpu_preferences,
    gpu::gles2::FeatureInfo* feature_info,
    std::unique_ptr<DirectContextProviderDelegate> delegate)
    : translator_cache_(gpu_preferences), delegate_(std::move(delegate)) {
  DCHECK(gl_context->IsCurrent(gl_surface.get()));

  auto limits = gpu::SharedMemoryLimits::ForMailboxContext();
  auto group = base::MakeRefCounted<gpu::gles2::ContextGroup>(
      gpu_preferences, gpu::gles2::PassthroughCommandDecoderSupported(),
      &mailbox_manager_, /*memory_tracker=*/nullptr, &translator_cache_,
      &completeness_cache_, feature_info, true, &image_manager_,
      /*image_factory=*/nullptr,
      /*progress_reporter=*/nullptr, gpu_feature_info_, &discardable_manager_,
      &passthrough_discardable_manager_, delegate_->GetSharedImageManager());

  auto command_buffer = std::make_unique<gpu::CommandBufferDirect>();

  std::unique_ptr<gpu::gles2::GLES2Decoder> decoder(
      gpu::gles2::GLES2Decoder::Create(command_buffer.get(),
                                       command_buffer->service(), &outputter_,
                                       group.get()));

  if (gpu_preferences.enable_gpu_service_logging)
    decoder->SetLogCommands(true);

  command_buffer->set_handler(decoder.get());

  gpu::ContextCreationAttribs attribs;
  attribs.alpha_size = supports_alpha ? 8 : 0;
  attribs.buffer_preserved = false;
  attribs.bind_generates_resource = true;
  attribs.fail_if_major_perf_caveat = false;
  attribs.lose_context_when_out_of_memory = true;
  attribs.context_type = gpu::CONTEXT_TYPE_OPENGLES2;

  context_result_ =
      decoder->Initialize(gl_surface, gl_context, gl_surface->IsOffscreen(),
                          gpu::gles2::DisallowedFeatures(), attribs);
  if (context_result_ != gpu::ContextResult::kSuccess)
    return;

  auto gles2_cmd_helper =
      std::make_unique<gpu::gles2::GLES2CmdHelper>(command_buffer.get());
  context_result_ = gles2_cmd_helper->Initialize(limits.command_buffer_size);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    decoder->Destroy(true);
    return;
  }
  // Client side Capabilities queries return reference, service side return
  // value. Here two sides are joined together.
  capabilities_ = decoder->GetCapabilities();

  auto transfer_buffer =
      std::make_unique<gpu::TransferBuffer>(gles2_cmd_helper.get());

  gles2_cmd_helper_ = std::move(gles2_cmd_helper);
  transfer_buffer_ = std::move(transfer_buffer);
  command_buffer_ = std::move(command_buffer);
  decoder_ = std::move(decoder);
  gl_context_ = std::move(gl_context);
  gl_surface_ = std::move(gl_surface);

  gles2_implementation_ = std::make_unique<gpu::gles2::GLES2Implementation>(
      gles2_cmd_helper_.get(), nullptr, transfer_buffer_.get(),
      attribs.bind_generates_resource, attribs.lose_context_when_out_of_memory,
      /*kSupportClientSideArrays=*/false, this);

  context_result_ = gles2_implementation_->Initialize(limits);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    Destroy();
    return;
  }

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "viz::DirectContextProvider", base::ThreadTaskRunnerHandle::Get());

  // TraceEndCHROMIUM is implicit when the context is destroyed
  gles2_implementation_->TraceBeginCHROMIUM("VizCompositor",
                                            "DisplayCompositor");
}

DirectContextProvider::~DirectContextProvider() {
  if (decoder_)
    Destroy();
}

void DirectContextProvider::Destroy() {
  DCHECK(decoder_);

  bool have_context = !decoder_->WasContextLost() &&
                      (gl_context_->IsCurrent(nullptr) ||
                       gl_context_->MakeCurrent(gl_surface_.get()));

  if (have_context && framebuffer_id_ != 0) {
    gles2_implementation_->DeleteFramebuffers(1, &framebuffer_id_);
    framebuffer_id_ = 0;
  }

  // The client gl interface might still be set to current global
  // interface. This will be cleaned up in ApplyContextReleased
  // with AutoCurrentContextRestore.
  gles2_implementation_.reset();
  gl_context_.reset();
  transfer_buffer_.reset();
  gles2_cmd_helper_.reset();

  decoder_->Destroy(have_context);
  decoder_.reset();

  command_buffer_.reset();
}

void DirectContextProvider::SetGLRendererCopierRequiredState(
    GLuint texture_client_id) {
  // Get into known state (see
  // SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider).
  gles2_implementation_->BindFramebuffer(GL_FRAMEBUFFER, 0);

  auto* group = decoder()->GetContextGroup();
  if (group->use_passthrough_cmd_decoder()) {
    // Matches state setting in
    // SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider when passthrough
    // is enabled so that client side and service side state match.
    //
    // TODO(backer): Use ANGLE API to force state reset once API is available.
    gles2_implementation_->UseProgram(0);
    gles2_implementation_->ActiveTexture(GL_TEXTURE0);
    gles2_implementation_->BindBuffer(GL_ARRAY_BUFFER, 0);
    gles2_implementation_->BindTexture(GL_TEXTURE_2D, 0);
  } else {
    decoder_->RestoreActiveTexture();
    decoder_->RestoreProgramBindings();
    decoder_->RestoreAllAttributes();
    decoder_->RestoreGlobalState();
    decoder_->RestoreBufferBindings();
  }

  // At this point |decoder_| cached state (if any, passthrough doesn't cache)
  // is synced with GLContext state. But GLES2Implementation caches some state
  // too and we need to make sure this are in sync with |decoder_| and context
  constexpr static std::initializer_list<GLuint> caps = {
      GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_BLEND};

  for (auto cap : caps) {
    if (gles2_implementation_->IsEnabled(cap))
      gles2_cmd_helper_->Enable(cap);
    else
      gles2_cmd_helper_->Disable(cap);
  }

  if (texture_client_id) {
    if (!framebuffer_id_)
      gles2_implementation_->GenFramebuffers(1, &framebuffer_id_);
    gles2_implementation_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
    gles2_implementation_->FramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_client_id,
        0);
    DCHECK_EQ(gles2_implementation_->CheckFramebufferStatus(GL_FRAMEBUFFER),
              static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE));
  }
}

gpu::gles2::TextureManager* DirectContextProvider::texture_manager() {
  return decoder_->GetContextGroup()->texture_manager();
}

void DirectContextProvider::AddRef() const {
  base::RefCountedThreadSafe<DirectContextProvider>::AddRef();
}

void DirectContextProvider::Release() const {
  base::RefCountedThreadSafe<DirectContextProvider>::Release();
}

gpu::ContextResult DirectContextProvider::BindToCurrentThread() {
  return context_result_;
}

gpu::gles2::GLES2Interface* DirectContextProvider::ContextGL() {
  return gles2_implementation_.get();
}

gpu::ContextSupport* DirectContextProvider::ContextSupport() {
  return gles2_implementation_.get();
}

class GrContext* DirectContextProvider::GrContext() {
  NOTREACHED();
  return nullptr;
}

gpu::SharedImageInterface* DirectContextProvider::SharedImageInterface() {
  return delegate_->GetSharedImageInterface();
}

ContextCacheController* DirectContextProvider::CacheController() {
  NOTREACHED();
  return nullptr;
}

base::Lock* DirectContextProvider::GetLock() {
  NOTREACHED();
  return nullptr;
}

const gpu::Capabilities& DirectContextProvider::ContextCapabilities() const {
  return capabilities_;
}

const gpu::GpuFeatureInfo& DirectContextProvider::GetGpuFeatureInfo() const {
  return gpu_feature_info_;
}

void DirectContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void DirectContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

void DirectContextProvider::OnContextLost() {
  // TODO(https://crbug.com/927460): Instrument this with a context loss UMA
  // stat shared with SkiaRenderer.
  for (auto& observer : observers_)
    observer.OnContextLost();
}

bool DirectContextProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_EQ(context_result_, gpu::ContextResult::kSuccess);

  gles2_implementation_->OnMemoryDump(args, pmd);
  gles2_cmd_helper_->OnMemoryDump(args, pmd);

  return true;
}

void DirectContextProvider::SetGpuControlClient(gpu::GpuControlClient*) {
  // The client is not currently called, so don't store it.
}

const gpu::Capabilities& DirectContextProvider::GetCapabilities() const {
  return capabilities_;
}

int32_t DirectContextProvider::CreateImage(ClientBuffer buffer,
                                           size_t width,
                                           size_t height) {
  NOTREACHED();
  return -1;
}

void DirectContextProvider::DestroyImage(int32_t id) {
  NOTREACHED();
}

void DirectContextProvider::SignalQuery(uint32_t query,
                                        base::OnceClosure callback) {
  decoder_->SetQueryCallback(query, std::move(callback));
}

void DirectContextProvider::CreateGpuFence(uint32_t gpu_fence_id,
                                           ClientGpuFence source) {
  NOTREACHED();
}

void DirectContextProvider::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  NOTREACHED();
}

void DirectContextProvider::SetLock(base::Lock*) {
  NOTREACHED();
}

void DirectContextProvider::EnsureWorkVisible() {
}

gpu::CommandBufferNamespace DirectContextProvider::GetNamespaceID() const {
  return delegate_->GetNamespaceID();
}

gpu::CommandBufferId DirectContextProvider::GetCommandBufferID() const {
  return delegate_->GetCommandBufferID();
}

void DirectContextProvider::FlushPendingWork() {
  NOTREACHED();
}

uint64_t DirectContextProvider::GenerateFenceSyncRelease() {
  return delegate_->GenerateFenceSyncRelease();
}

bool DirectContextProvider::IsFenceSyncReleased(uint64_t release) {
  NOTREACHED();
  return false;
}

void DirectContextProvider::SignalSyncToken(const gpu::SyncToken& sync_token,
                                            base::OnceClosure callback) {
  delegate_->SignalSyncToken(sync_token, std::move(callback));
}

void DirectContextProvider::WaitSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED();
}

bool DirectContextProvider::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken& sync_token) {
  return false;
}

void DirectContextProvider::SetDisplayTransform(
    gfx::OverlayTransform transform) {
  NOTREACHED();
}

GLuint DirectContextProvider::GenClientTextureId() {
  const auto& share_group = gles2_implementation_->share_group();
  auto* id_handler =
      share_group->GetIdHandler(gpu::gles2::SharedIdNamespaces::kTextures);
  GLuint client_id;
  id_handler->MakeIds(gles2_implementation_.get(), 0, 1, &client_id);
  return client_id;
}

void DirectContextProvider::DeleteClientTextureId(GLuint client_id) {
  gles2_implementation_->DeleteTextures(1, &client_id);
}

void DirectContextProvider::MarkContextLost() {
  if (!decoder_->WasContextLost()) {
    decoder_->MarkContextLost(gpu::error::kUnknown);
    command_buffer_->service()->SetParseError(gpu::error::kLostContext);
    OnContextLost();
  }
}

void DirectContextProvider::FinishQueries() {
  if (decoder_->HasPendingQueries())
    gles2_implementation_->Finish();
}

}  // namespace viz

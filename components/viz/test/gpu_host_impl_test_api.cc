// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/gpu_host_impl_test_api.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "build/build_config.h"

namespace viz {

gpu::GPUInfo GpuHostImplTestApi::HookDelegateBase::GetGPUInfo() const {
  return original_delegate_->GetGPUInfo();
}

gpu::GpuFeatureInfo GpuHostImplTestApi::HookDelegateBase::GetGpuFeatureInfo()
    const {
  return original_delegate_->GetGpuFeatureInfo();
}

void GpuHostImplTestApi::HookDelegateBase::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const std::optional<gpu::GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info) {
  original_delegate_->DidInitialize(
      gpu_info, gpu_feature_info, gpu_info_for_hardware_gpu,
      gpu_feature_info_for_hardware_gpu, gpu_extra_info);
}

void GpuHostImplTestApi::HookDelegateBase::DidFailInitialize() {
  original_delegate_->DidFailInitialize();
}

void GpuHostImplTestApi::HookDelegateBase::DidCreateContextSuccessfully() {
  original_delegate_->DidCreateContextSuccessfully();
}

void GpuHostImplTestApi::HookDelegateBase::MaybeShutdownGpuProcess() {
  original_delegate_->MaybeShutdownGpuProcess();
}

void GpuHostImplTestApi::HookDelegateBase::DidUpdateGPUInfo(
    const gpu::GPUInfo& gpu_info) {
  original_delegate_->DidUpdateGPUInfo(gpu_info);
}

#if BUILDFLAG(IS_WIN)
void GpuHostImplTestApi::HookDelegateBase::DidUpdateOverlayInfo(
    const gpu::OverlayInfo& overlay_info) {
  original_delegate_->DidUpdateOverlayInfo(overlay_info);
}

void GpuHostImplTestApi::HookDelegateBase::DidUpdateDXGIInfo(
    gfx::mojom::DXGIInfoPtr dxgi_info) {
  original_delegate_->DidUpdateDXGIInfo(std::move(dxgi_info));
}
#endif

void GpuHostImplTestApi::HookDelegateBase::BlockDomainsFrom3DAPIs(
    const std::set<GURL>& urls,
    gpu::DomainGuilt guilt) {
  original_delegate_->BlockDomainsFrom3DAPIs(urls, guilt);
}

std::string GpuHostImplTestApi::HookDelegateBase::GetIsolationKey(
    int32_t client_id,
    const blink::WebGPUExecutionContextToken& token) {
  return original_delegate_->GetIsolationKey(client_id, token);
}

void GpuHostImplTestApi::HookDelegateBase::DisableGpuCompositing() {
  original_delegate_->DisableGpuCompositing();
}

bool GpuHostImplTestApi::HookDelegateBase::GpuAccessAllowed() const {
  return original_delegate_->GpuAccessAllowed();
}

gpu::GpuDiskCacheFactory*
GpuHostImplTestApi::HookDelegateBase::GetGpuDiskCacheFactory() {
  return original_delegate_->GetGpuDiskCacheFactory();
}

void GpuHostImplTestApi::HookDelegateBase::RecordLogMessage(
    int32_t severity,
    const std::string& header,
    const std::string& message) {
  original_delegate_->RecordLogMessage(severity, header, message);
}

void GpuHostImplTestApi::HookDelegateBase::BindDiscardableMemoryReceiver(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver) {
  original_delegate_->BindDiscardableMemoryReceiver(std::move(receiver));
}

void GpuHostImplTestApi::HookDelegateBase::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  original_delegate_->BindInterface(interface_name, std::move(interface_pipe));
}

#if BUILDFLAG(IS_OZONE)
void GpuHostImplTestApi::HookDelegateBase::TerminateGpuProcess(
    const std::string& message) {
  original_delegate_->TerminateGpuProcess(message);
}
#endif

GpuHostImplTestApi::GpuHostImplTestApi(GpuHostImpl* gpu_host)
    : gpu_host_(gpu_host) {}

GpuHostImplTestApi::~GpuHostImplTestApi() {
  if (hook_delegate_)
    gpu_host_->delegate_ = hook_delegate_->original_delegate();
}

void GpuHostImplTestApi::FlushRemoteForTesting() {
  gpu_host_->gpu_service_remote_.FlushForTesting();
}

void GpuHostImplTestApi::SetGpuService(
    mojo::Remote<mojom::GpuService> gpu_service) {
  gpu_host_->gpu_service_remote_ = std::move(gpu_service);
}

void GpuHostImplTestApi::HookDelegate(
    std::unique_ptr<HookDelegateBase> delegate) {
  DCHECK(delegate);

  GpuHostImpl::Delegate* original_delegate =
      hook_delegate_ ? hook_delegate_->original_delegate()
                     : gpu_host_->delegate_.get();

  hook_delegate_ = std::move(delegate);
  hook_delegate_->set_original_delegate(original_delegate);

  gpu_host_->delegate_ = hook_delegate_.get();
}

}  // namespace viz

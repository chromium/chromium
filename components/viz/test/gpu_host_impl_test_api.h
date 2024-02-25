// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_
#define COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/host/gpu_host_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace viz {
class GpuHostImpl;

class GpuHostImplTestApi {
 public:
  // Base class for hook delegates used by HookDelegate(). The default behavior
  // is forwarding the call to the original delegate.
  class HookDelegateBase : public GpuHostImpl::Delegate {
   public:
    HookDelegateBase() = default;

    HookDelegateBase(HookDelegateBase&) = delete;
    HookDelegateBase& operator=(HookDelegateBase&) = delete;

    ~HookDelegateBase() override = default;

    void set_original_delegate(GpuHostImpl::Delegate* original_delegate) {
      original_delegate_ = original_delegate;
    }

    GpuHostImpl::Delegate* original_delegate() { return original_delegate_; }

    // GpuHostImpl::Delegate
    gpu::GPUInfo GetGPUInfo() const override;
    gpu::GpuFeatureInfo GetGpuFeatureInfo() const override;
    void DidInitialize(
        const gpu::GPUInfo& gpu_info,
        const gpu::GpuFeatureInfo& gpu_feature_info,
        const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
        const std::optional<gpu::GpuFeatureInfo>&
            gpu_feature_info_for_hardware_gpu,
        const gfx::GpuExtraInfo& gpu_extra_info) override;
    void DidFailInitialize() override;
    void DidCreateContextSuccessfully() override;
    void MaybeShutdownGpuProcess() override;
    void DidUpdateGPUInfo(const gpu::GPUInfo& gpu_info) override;
#if BUILDFLAG(IS_WIN)
    void DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) override;
    void DidUpdateDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info) override;
#endif
    void BlockDomainsFrom3DAPIs(const std::set<GURL>& urls,
                                gpu::DomainGuilt guilt) override;
    std::string GetIsolationKey(
        int32_t client_id,
        const blink::WebGPUExecutionContextToken& token) override;
    void DisableGpuCompositing() override;
    bool GpuAccessAllowed() const override;
    gpu::GpuDiskCacheFactory* GetGpuDiskCacheFactory() override;
    void RecordLogMessage(int32_t severity,
                          const std::string& header,
                          const std::string& message) override;
    void BindDiscardableMemoryReceiver(
        mojo::PendingReceiver<
            discardable_memory::mojom::DiscardableSharedMemoryManager> receiver)
        override;
    void BindInterface(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
#if BUILDFLAG(IS_OZONE)
    void TerminateGpuProcess(const std::string& message) override;
#endif

   private:
    raw_ptr<GpuHostImpl::Delegate> original_delegate_ = nullptr;
  };

  explicit GpuHostImplTestApi(GpuHostImpl* gpu_host);

  GpuHostImplTestApi(const GpuHostImplTestApi&) = delete;
  GpuHostImplTestApi& operator=(const GpuHostImplTestApi&) = delete;

  ~GpuHostImplTestApi();

  // Waits until all messages to the mojo::Remote<mojom::GpuService> have been
  // processed.
  void FlushRemoteForTesting();
  void SetGpuService(mojo::Remote<mojom::GpuService> gpu_service);

  // Hooks the delegate of `gpu_host_`. The hook is removed when this object
  // destructs.
  void HookDelegate(std::unique_ptr<HookDelegateBase> delegate);

 private:
  raw_ptr<GpuHostImpl, DanglingUntriaged> gpu_host_;

  std::unique_ptr<HookDelegateBase> hook_delegate_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_

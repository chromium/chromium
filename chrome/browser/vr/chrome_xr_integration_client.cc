// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/chrome_xr_integration_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/xr_install_helper.h"
#include "content/public/common/content_switches.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"
#elif BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_GVR_SERVICES)
#include "chrome/browser/android/vr/gvr_install_helper.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#endif
#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/android/vr/ar_jni_headers/ArCompositorDelegateProviderImpl_jni.h"
#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "components/webxr/android/arcore_device_provider.h"
#include "components/webxr/android/arcore_install_helper.h"
#endif  // BUILDFLAG(ENABLE_ARCORE)
#if BUILDFLAG(ENABLE_CARDBOARD)
#include "chrome/browser/android/vr/vr_jni_headers/VrCompositorDelegateProviderImpl_jni.h"
#include "components/webxr/android/cardboard_device_provider.h"
#include "components/webxr/android/vr_compositor_delegate_provider.h"
#endif
#if BUILDFLAG(ENABLE_OPENXR)
#include "components/webxr/android/openxr_device_provider.h"
#endif
#endif  // BUILDFLAG(IS_WIN)

namespace {

constexpr char kWebXrVideoCaptureDeviceId[] = "WebXRVideoCaptureDevice:-1";
constexpr char kWebXrVideoCaptureDeviceName[] = "WebXRVideoCaptureDevice";
constexpr char kWebXrMediaStreamLabel[] = "WebXR Raw Camera Access";

class CameraIndicationObserver : public content::BrowserXRRuntime::Observer {
 public:
  void WebXRCameraInUseChanged(content::WebContents* web_contents,
                               bool in_use) override {
    DVLOG(3) << __func__ << ": web_contents=" << web_contents
             << ", in_use=" << in_use << ", num_runtimes_with_camera_in_use_="
             << num_runtimes_with_camera_in_use_ << ", ui_=" << ui_.get();
    // If `in_use` is true, we need to have a non-null `web_contents` to be able
    // to register the media stream:
    DCHECK(!in_use || web_contents);

    if (in_use) {
      num_runtimes_with_camera_in_use_++;
    } else {
      DCHECK_GT(num_runtimes_with_camera_in_use_, 0u);
      num_runtimes_with_camera_in_use_--;
    }

    if (num_runtimes_with_camera_in_use_ && !ui_) {
      DCHECK(web_contents);

      blink::mojom::StreamDevices devices;
      devices.video_device = blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          kWebXrVideoCaptureDeviceId, kWebXrVideoCaptureDeviceName);

      ui_ = MediaCaptureDevicesDispatcher::GetInstance()
                ->GetMediaStreamCaptureIndicator()
                ->RegisterMediaStream(web_contents, devices);
      DCHECK(ui_);
    }

    if (num_runtimes_with_camera_in_use_) {
      ui_->OnStarted({}, {}, kWebXrMediaStreamLabel, {}, {});
    } else {
      ui_->OnDeviceStopped(kWebXrMediaStreamLabel, {});
      ui_ = nullptr;
    }
  }

 private:
  size_t num_runtimes_with_camera_in_use_ = 0;
  std::unique_ptr<content::MediaStreamUI> ui_;
};

#if BUILDFLAG(IS_ANDROID)
// If none of the runtimes are enabled, this function will be unused.
// This is a bit more scalable than wrapping it in all the typedefs
[[maybe_unused]] bool IsEnabled(const base::CommandLine* command_line,
                                const std::string& name,
                                const base::Feature* maybe_feature = nullptr) {
  // If we don't have a forced runtime we just need to check if the feature is
  // enabled.
  if (!command_line->HasSwitch(switches::kWebXrForceRuntime)) {
    // Either we were passed a feature, in which case we need to check if it's
    // enabled. Or we weren't, in which case the feature should be enabled.
    return maybe_feature ? base::FeatureList::IsEnabled(*maybe_feature) : true;
  }

  return (base::CompareCaseInsensitiveASCII(
              command_line->GetSwitchValueASCII(switches::kWebXrForceRuntime),
              name) == 0);
}
#endif
}  // namespace

namespace vr {

std::unique_ptr<content::XrInstallHelper>
ChromeXrIntegrationClient::GetInstallHelper(
    device::mojom::XRDeviceId device_id) {
  switch (device_id) {
#if BUILDFLAG(ENABLE_GVR_SERVICES)
    case device::mojom::XRDeviceId::GVR_DEVICE_ID:
      return std::make_unique<GvrInstallHelper>();
#endif  // BUILDFLAG(ENABLE_GVR_SERVICES)
#if BUILDFLAG(ENABLE_ARCORE)
    case device::mojom::XRDeviceId::ARCORE_DEVICE_ID:
      return std::make_unique<webxr::ArCoreInstallHelper>();
#endif  // BUILDFLAG(ENABLE_ARCORE)
    default:
      return nullptr;
  }
}

content::XRProviderList ChromeXrIntegrationClient::GetAdditionalProviders() {
  content::XRProviderList providers;

#if BUILDFLAG(IS_ANDROID)
  // May be unused if all runtimes are disabled.
  [[maybe_unused]] bool preferred_vr_runtime_added = false;
  [[maybe_unused]] const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(ENABLE_OPENXR)
  if (!preferred_vr_runtime_added &&
      IsEnabled(command_line, switches::kWebXrRuntimeOpenXr,
                &device::features::kOpenXR)) {
    providers.emplace_back(std::make_unique<webxr::OpenXrDeviceProvider>());
    preferred_vr_runtime_added = true;
  }
#endif  // BUILDFLAG(ENABLE_OPENXR)
#if BUILDFLAG(ENABLE_CARDBOARD)
  if (!preferred_vr_runtime_added &&
      IsEnabled(command_line, switches::kWebXrRuntimeCardboard,
                &device::features::kEnableCardboard)) {
    base::android::ScopedJavaLocalRef<jobject>
        j_vr_compositor_delegate_provider =
            vr::Java_VrCompositorDelegateProviderImpl_Constructor(
                base::android::AttachCurrentThread());
    providers.emplace_back(std::make_unique<webxr::CardboardDeviceProvider>(
        std::make_unique<webxr::VrCompositorDelegateProvider>(
            std::move(j_vr_compositor_delegate_provider))));
    preferred_vr_runtime_added = true;
  }
#endif  // BUILDFLAG(ENABLE_CARDBOARD)
#if BUILDFLAG(ENABLE_GVR_SERVICES)
  if (!preferred_vr_runtime_added &&
      IsEnabled(command_line, switches::kWebXrRuntimeGVR)) {
    providers.push_back(std::make_unique<device::GvrDeviceProvider>());
    preferred_vr_runtime_added = true;
  }
#endif  // BUILDFLAG(ENABLE_GVR_SERVICES)
#if BUILDFLAG(ENABLE_ARCORE)
  base::android::ScopedJavaLocalRef<jobject> j_ar_compositor_delegate_provider =
      vr::Java_ArCompositorDelegateProviderImpl_Constructor(
          base::android::AttachCurrentThread());

  providers.push_back(std::make_unique<webxr::ArCoreDeviceProvider>(
      std::make_unique<webxr::ArCompositorDelegateProvider>(
          std::move(j_ar_compositor_delegate_provider))));
#endif  // BUILDFLAG(ENABLE_ARCORE)
#endif  // BUILDFLAG(IS_ANDROID)

  return providers;
}

std::unique_ptr<content::BrowserXRRuntime::Observer>
ChromeXrIntegrationClient::CreateRuntimeObserver() {
  DVLOG(3) << __func__;
  return std::make_unique<CameraIndicationObserver>();
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<content::VrUiHost> ChromeXrIntegrationClient::CreateVrUiHost(
    device::mojom::XRDeviceId device_id,
    mojo::PendingRemote<device::mojom::XRCompositorHost> compositor) {
  return std::make_unique<VRUiHostImpl>(device_id, std::move(compositor));
}
#endif
}  // namespace vr

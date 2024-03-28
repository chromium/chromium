// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_default.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/lru_renderer_cache.h"
#include "chromecast/browser/renderer_prelauncher.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/graphics/cast_screen.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace chromecast {

namespace {

std::unique_ptr<content::WebContents> CreateWebContents(
    content::BrowserContext* browser_context,
    scoped_refptr<content::SiteInstance> site_instance,
    const mojom::CastWebViewParams& params) {
  DCHECK(browser_context);
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  create_params.site_instance = site_instance;

  return content::WebContents::Create(create_params);
}

std::unique_ptr<RendererPrelauncher> TakeOrCreatePrelauncher(
    const GURL& prelaunch_url,
    mojom::RendererPool renderer_pool,
    CastWebService* web_service) {
  if (!prelaunch_url.is_valid()) {
    return nullptr;
  }
  if (renderer_pool == mojom::RendererPool::OVERLAY) {
    return web_service->overlay_renderer_cache()->TakeRendererPrelauncher(
        prelaunch_url);
  }
  return std::make_unique<RendererPrelauncher>(web_service->browser_context(),
                                               prelaunch_url);
}

scoped_refptr<content::SiteInstance> Prelaunch(
    RendererPrelauncher* prelauncher) {
  if (!prelauncher) {
    return nullptr;
  }
  prelauncher->Prelaunch();
  return prelauncher->site_instance();
}

#if defined(USE_AURA)
constexpr gfx::Rect k720pDimensions(0, 0, 1280, 720);
#endif

}  // namespace

CastWebViewDefault::CastWebViewDefault(
    mojom::CastWebViewParamsPtr params,
    CastWebService* web_service,
    content::BrowserContext* browser_context,
    std::unique_ptr<CastContentWindow> cast_content_window)
    : params_(std::move(params)),
      web_service_(web_service),
      renderer_prelauncher_(TakeOrCreatePrelauncher(params_->prelaunch_url,
                                                    params_->renderer_pool,
                                                    web_service_)),
      site_instance_(Prelaunch(renderer_prelauncher_.get())),
      web_contents_(
          CreateWebContents(browser_context, site_instance_, *params_)),
      cast_web_contents_(web_contents_.get(), params_->Clone()),
      window_(cast_content_window
                  ? std::move(cast_content_window)
                  : web_service->CreateWindow(params_->Clone())) {
  DCHECK(web_service_);
  DCHECK(window_);
  window_->SetCastWebContents(&cast_web_contents_);
  web_contents_->SetDelegate(this);
#if defined(USE_AURA)
  web_contents_->GetNativeView()->SetName(params_->activity_id);
  if (params_->force_720p_resolution) {
    const auto primary_display =
        display::Screen::GetScreen()->GetPrimaryDisplay();

    // Force scale factor to 1.0 and screen bounds to 720p.
    // When performed prior to the creation of the web view this causes blink to
    // render at a 1.0 pixel ratio but the compositor still scales out at 1.5,
    // increasing performance on 1080p displays (at the expense of visual
    // quality).
    shell::CastBrowserProcess::GetInstance()
        ->cast_screen()
        ->OverridePrimaryDisplaySettings(k720pDimensions, 1.0,
                                         primary_display.rotation());
  }
#endif
}

CastWebViewDefault::~CastWebViewDefault() {
  if (renderer_prelauncher_ && params_->prelaunch_url.is_valid() &&
      params_->renderer_pool == mojom::RendererPool::OVERLAY) {
    web_service_->overlay_renderer_cache()->ReleaseRendererPrelauncher(
        params_->prelaunch_url);
  }
}

CastContentWindow* CastWebViewDefault::window() const {
  return window_.get();
}

content::WebContents* CastWebViewDefault::web_contents() const {
  return web_contents_.get();
}

CastWebContents* CastWebViewDefault::cast_web_contents() {
  return &cast_web_contents_;
}

base::TimeDelta CastWebViewDefault::shutdown_delay() const {
  return params_->shutdown_delay;
}

void CastWebViewDefault::OwnerDestroyed() {
#if defined(USE_AURA)
  if (params_->force_720p_resolution) {
    shell::CastBrowserProcess::GetInstance()
        ->cast_screen()
        ->RestorePrimaryDisplaySettings();
  }
#endif
}

void CastWebViewDefault::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  window_.reset();  // Window destructor requires live web_contents on Android.
  // This will signal to the owner that |web_contents_| is no longer in use,
  // permitting the owner to tear down.
  cast_web_contents_.Stop(net::OK);
}

content::WebContents* CastWebViewDefault::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  LOG(INFO) << "Change url: " << params.url;
  // If source is NULL which means current tab, use web_contents_ of this class.
  if (!source)
    source = web_contents_.get();
  DCHECK_EQ(source, web_contents_.get());
  // We don't want to create another web_contents. Load url only when source is
  // specified.
  auto navigation_handle = source->GetController().LoadURL(
      params.url, params.referrer, params.transition, params.extra_headers);

  if (navigation_handle_callback && navigation_handle) {
    std::move(navigation_handle_callback).Run(*navigation_handle);
  }
  return source;
}

void CastWebViewDefault::ActivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents_.get());
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

bool CastWebViewDefault::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (!chromecast::IsFeatureEnabled(kAllowUserMediaAccess) &&
      !params_->allow_media_access) {
    LOG(WARNING) << __func__ << ": media access is disabled.";
    return false;
  }
  return true;
}

bool CastWebViewDefault::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  if (!params_->log_js_console_messages)
    return true;
  std::u16string single_line_message;
  // Mult-line message is not friendly to dumpstate redact.
  base::ReplaceChars(message, u"\n", u"\\n ", &single_line_message);
  logging::LogMessage("CONSOLE", line_no, ::logging::LOGGING_INFO).stream()
      << params_->log_prefix << ": \"" << single_line_message
      << "\", source: " << source_id << " (" << line_no << ")";
  return true;
}

const blink::MediaStreamDevice* GetRequestedDeviceOrDefault(
    const blink::MediaStreamDevices& devices,
    const std::vector<std::string>& requested_device_ids) {
  if (!requested_device_ids.empty() && !requested_device_ids.front().empty()) {
    auto it = base::ranges::find(devices, requested_device_ids.front(),
                                 &blink::MediaStreamDevice::id);
    return it != devices.end() ? &(*it) : nullptr;
  }

  if (!devices.empty())
    return &devices[0];

  return nullptr;
}

void CastWebViewDefault::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (!chromecast::IsFeatureEnabled(kAllowUserMediaAccess) &&
      !params_->allow_media_access) {
    LOG(WARNING) << __func__ << ": media access is disabled.";
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  auto audio_devices =
      content::MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
  auto video_devices =
      content::MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
  DVLOG(2) << __func__ << " audio_devices=" << audio_devices.size()
           << " video_devices=" << video_devices.size();

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];
  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    const blink::MediaStreamDevice* device = GetRequestedDeviceOrDefault(
        audio_devices, request.requested_audio_device_ids);
    if (device) {
      DVLOG(1) << __func__ << "Using audio device: id=" << device->id
               << " name=" << device->name;
      devices.audio_device = *device;
    }
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    const blink::MediaStreamDevice* device = GetRequestedDeviceOrDefault(
        video_devices, request.requested_video_device_ids);
    if (device) {
      DVLOG(1) << __func__ << "Using video device: id=" << device->id
               << " name=" << device->name;
      devices.video_device = *device;
    }
  }

  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<content::MediaStreamUI>());
}

bool CastWebViewDefault::ShouldAllowRunningInsecureContent(
    content::WebContents* /* web_contents */,
    bool allowed_per_prefs,
    const url::Origin& /* origin */,
    const GURL& /* resource_url */) {
  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
      params_->activity_id, params_->session_id, params_->sdk_version,
      "Cast.Platform.AppRunningInsecureContent");
  return allowed_per_prefs;
}

}  // namespace chromecast

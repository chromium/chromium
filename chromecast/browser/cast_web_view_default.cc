// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_default.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
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
    scoped_refptr<content::SiteInstance> site_instance) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.site_instance = site_instance;
  return content::WebContents::Create(create_params);
}

}  // namespace

CastWebViewDefault::CastWebViewDefault(
    const CreateParams& params,
    CastWebService* web_service,
    content::BrowserContext* browser_context,
    scoped_refptr<content::SiteInstance> site_instance,
    std::unique_ptr<CastContentWindow> cast_content_window)
    : CastWebView(params),
      web_service_(web_service),
      browser_context_(browser_context),
      site_instance_(std::move(site_instance)),
      activity_id_(params.activity_id),
      session_id_(params.window_params.session_id),
      sdk_version_(params.sdk_version),
      allow_media_access_(params.allow_media_access),
      log_prefix_(params.log_prefix),
      web_contents_(CreateWebContents(browser_context_, site_instance_)),
      cast_web_contents_(web_contents_.get(), params.web_contents_params),
      window_(cast_content_window
                  ? std::move(cast_content_window)
                  : web_service->CreateWindow(params.window_params)),
      resize_window_when_navigation_starts_(true) {
  DCHECK(web_service_);
  DCHECK(browser_context_);
  DCHECK(window_);
  content::WebContentsObserver::Observe(web_contents_.get());

  web_contents_->SetDelegate(this);
#if defined(USE_AURA)
  web_contents_->GetNativeView()->SetName(params.activity_id);
#endif

#if BUILDFLAG(IS_ANDROID_THINGS)
  // Configure the ducking multiplier for AThings speakers. We don't want the
  // Chromium MediaSession to duck since we are doing our own ducking.
  constexpr double kDuckingMultiplier = 1.0;
  content::MediaSession::Get(web_contents_.get())
      ->SetDuckingVolumeMultiplier(kDuckingMultiplier);
#endif
}

CastWebViewDefault::~CastWebViewDefault() {}

CastContentWindow* CastWebViewDefault::window() const {
  return window_.get();
}

content::WebContents* CastWebViewDefault::web_contents() const {
  return web_contents_.get();
}

CastWebContents* CastWebViewDefault::cast_web_contents() {
  return &cast_web_contents_;
}

void CastWebViewDefault::LoadUrl(GURL url) {
  cast_web_contents_.LoadUrl(url);
}

void CastWebViewDefault::ClosePage() {
  content::WebContentsObserver::Observe(nullptr);
  cast_web_contents_.ClosePage();
}

void CastWebViewDefault::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());
  window_.reset();  // Window destructor requires live web_contents on Android.
  // This will signal to the owner that |web_contents_| is no longer in use,
  // permitting the owner to tear down.
  cast_web_contents_.Stop(net::OK);
}

void CastWebViewDefault::InitializeWindow(mojom::ZOrder z_order,
                                          VisibilityPriority initial_priority) {
  if (!window_)
    return;
  window_->CreateWindowForWebContents(&cast_web_contents_, z_order,
                                      initial_priority);
  web_contents_->Focus();
}

void CastWebViewDefault::GrantScreenAccess() {
  if (!window_)
    return;
  window_->GrantScreenAccess();
}

void CastWebViewDefault::RevokeScreenAccess() {
  resize_window_when_navigation_starts_ = false;
  if (!window_)
    return;
  window_->RevokeScreenAccess();
}

content::WebContents* CastWebViewDefault::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  LOG(INFO) << "Change url: " << params.url;
  // If source is NULL which means current tab, use web_contents_ of this class.
  if (!source)
    source = web_contents_.get();
  DCHECK_EQ(source, web_contents_.get());
  // We don't want to create another web_contents. Load url only when source is
  // specified.
  source->GetController().LoadURL(params.url, params.referrer,
                                  params.transition, params.extra_headers);
  return source;
}

void CastWebViewDefault::ActivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents_.get());
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

bool CastWebViewDefault::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  if (!chromecast::IsFeatureEnabled(kAllowUserMediaAccess) &&
      !allow_media_access_) {
    LOG(WARNING) << __func__ << ": media access is disabled.";
    return false;
  }
  return true;
}

bool CastWebViewDefault::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  base::string16 single_line_message;
  // Mult-line message is not friendly to dumpstate redact.
  base::ReplaceChars(message, base::ASCIIToUTF16("\n"),
                     base::ASCIIToUTF16("\\n "), &single_line_message);
  logging::LogMessage("CONSOLE", line_no, ::logging::LOG_INFO).stream()
      << log_prefix_ << ": \"" << single_line_message
      << "\", source: " << source_id << " (" << line_no << ")";
  return true;
}

const blink::MediaStreamDevice* GetRequestedDeviceOrDefault(
    const blink::MediaStreamDevices& devices,
    const std::string& requested_device_id) {
  if (!requested_device_id.empty()) {
    auto it = std::find_if(
        devices.begin(), devices.end(),
        [requested_device_id](const blink::MediaStreamDevice& device) {
          return device.id == requested_device_id;
        });
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
      !allow_media_access_) {
    LOG(WARNING) << __func__ << ": media access is disabled.";
    std::move(callback).Run(
        blink::MediaStreamDevices(),
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

  blink::MediaStreamDevices devices;
  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    const blink::MediaStreamDevice* device = GetRequestedDeviceOrDefault(
        audio_devices, request.requested_audio_device_id);
    if (device) {
      DVLOG(1) << __func__ << "Using audio device: id=" << device->id
               << " name=" << device->name;
      devices.push_back(*device);
    }
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    const blink::MediaStreamDevice* device = GetRequestedDeviceOrDefault(
        video_devices, request.requested_video_device_id);
    if (device) {
      DVLOG(1) << __func__ << "Using video device: id=" << device->id
               << " name=" << device->name;
      devices.push_back(*device);
    }
  }

  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<content::MediaStreamUI>());
}

std::unique_ptr<content::BluetoothChooser>
CastWebViewDefault::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  std::unique_ptr<content::BluetoothChooser> chooser;
  if (delegate_) {
    chooser = delegate_->RunBluetoothChooser(frame, event_handler);
  }
  return chooser
             ? std::move(chooser)
             : WebContentsDelegate::RunBluetoothChooser(frame, event_handler);
}

bool CastWebViewDefault::ShouldAllowRunningInsecureContent(
    content::WebContents* /* web_contents */,
    bool allowed_per_prefs,
    const url::Origin& /* origin */,
    const GURL& /* resource_url */) {
  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEvent(
      activity_id_, session_id_, sdk_version_,
      "Cast.Platform.AppRunningInsecureContent");
  return allowed_per_prefs;
}

void CastWebViewDefault::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!resize_window_when_navigation_starts_) {
    return;
  }
  resize_window_when_navigation_starts_ = false;

#if defined(USE_AURA)
  // Resize window
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  aura::Window* content_window = web_contents()->GetNativeView();
  content_window->SetBounds(
      gfx::Rect(display_size.width(), display_size.height()));
#endif
}

}  // namespace chromecast

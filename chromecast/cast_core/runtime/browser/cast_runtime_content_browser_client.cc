// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/service/cast_service_simple.h"
#include "chromecast/browser/webui/constants.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "chromecast/cast_core/runtime/browser/runtime_service_impl.h"
#include "chromecast/common/cors_exempt_headers.h"
#include "chromecast/media/base/video_plane_controller.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "media/base/cdm_factory.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace chromecast {

namespace {

// CastServiceSimple impl for Cast Core that allows the runtime service to start
// up and tear down.
class CoreCastService : public shell::CastServiceSimple {
 public:
  CoreCastService(
      CastWebService* web_service,
      cast_receiver::ContentBrowserClientMixins& cast_browser_client_mixins_)
      : CastServiceSimple(web_service),
        runtime_service_(
            std::make_unique<RuntimeServiceImpl>(cast_browser_client_mixins_,
                                                 *web_service)) {}

  // CastServiceSimple overrides:
  void StartInternal() override {
    if (!runtime_service_->Start().ok()) {
      base::Process::TerminateCurrentProcessImmediately(1);
    }
  }

  void StopInternal() override { runtime_service_->Stop(); }

  void FinalizeInternal() override { runtime_service_.reset(); }

 private:
  std::unique_ptr<RuntimeServiceImpl> runtime_service_;
};

}  // namespace

CastRuntimeContentBrowserClient::CastRuntimeContentBrowserClient(
    CastFeatureListCreator* feature_list_creator)
    : shell::CastContentBrowserClient(feature_list_creator),
      cast_browser_client_mixins_(
          cast_receiver::ContentBrowserClientMixins::Create(base::BindRepeating(
              &CastRuntimeContentBrowserClient::GetSystemNetworkContext,
              base::Unretained(this)))) {
  cast_browser_client_mixins_->AddStreamingResolutionObserver(&observer_);
  cast_browser_client_mixins_->AddApplicationStateObserver(&observer_);
}

CastRuntimeContentBrowserClient::~CastRuntimeContentBrowserClient() {
  cast_browser_client_mixins_->RemoveStreamingResolutionObserver(&observer_);
  cast_browser_client_mixins_->RemoveApplicationStateObserver(&observer_);
}

std::unique_ptr<CastService> CastRuntimeContentBrowserClient::CreateCastService(
    content::BrowserContext* browser_context,
    CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
    PrefService* pref_service,
    media::VideoPlaneController* video_plane_controller,
    CastWindowManager* window_manager,
    CastWebService* web_service,
    DisplaySettingsManager* display_settings_manager) {
  observer_.SetVideoPlaneController(video_plane_controller);

  // Unretained() is safe here because this instance will outlive CastService.
  return std::make_unique<CoreCastService>(web_service,
                                           *cast_browser_client_mixins_);
}

std::unique_ptr<::media::CdmFactory>
CastRuntimeContentBrowserClient::CreateCdmFactory(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

void CastRuntimeContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  CastContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                           child_process_id);

  base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();
  if (browser_command_line->HasSwitch(switches::kLogFile) &&
      !command_line->HasSwitch(switches::kLogFile)) {
    static const char* const kPath[] = {switches::kLogFile};
    command_line->CopySwitchesFrom(*browser_command_line, kPath);
  }
}

bool CastRuntimeContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return origin.host() == kCastWebUIHomeHost;
}

bool CastRuntimeContentBrowserClient::IsBufferingEnabled() {
  return observer_.IsBufferingEnabled();
}

void CastRuntimeContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  cast_browser_client_mixins_->OnWebContentsCreated(web_contents);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CastRuntimeContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  return cast_browser_client_mixins_->CreateURLLoaderThrottles(
      std::move(wc_getter), frame_tree_node_id,
      base::BindRepeating(&IsCorsExemptHeader));
}

CastRuntimeContentBrowserClient::Observer::~Observer() = default;

void CastRuntimeContentBrowserClient::Observer::SetVideoPlaneController(
    media::VideoPlaneController* video_plane_controller) {
  video_plane_controller_ = video_plane_controller;
}

bool CastRuntimeContentBrowserClient::Observer::IsBufferingEnabled() const {
  return is_buffering_enabled_.load();
}

void CastRuntimeContentBrowserClient::Observer::OnForegroundApplicationChanged(
    cast_receiver::RuntimeApplication* app) {
  bool enabled = true;
  // Buffering must be disabled for streaming applications.
  if (app && app->IsStreamingApplication()) {
    enabled = false;
  }

  is_buffering_enabled_.store(enabled);
  DLOG(INFO) << "Buffering is " << (enabled ? "enabled" : "disabled");
}

void CastRuntimeContentBrowserClient::Observer::OnStreamingResolutionChanged(
    const gfx::Rect& size,
    const ::media::VideoTransformation& transformation) {
  if (video_plane_controller_) {
    video_plane_controller_->SetGeometryFromMediaType(size, transformation);
  }
}

}  // namespace chromecast

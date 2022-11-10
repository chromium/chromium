// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/service/cast_service_simple.h"
#include "chromecast/browser/webui/constants.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"
#include "chromecast/cast_core/runtime/browser/runtime_service_impl.h"
#include "chromecast/media/base/video_plane_controller.h"
#include "components/cast_receiver/browser/public/application_client.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "content/public/common/content_switches.h"
#include "media/base/cdm_factory.h"

namespace chromecast {

namespace {

// CastServiceSimple impl for Cast Core that allows correct dispatcher start up
// and tear down.
class CoreCastService : public shell::CastServiceSimple {
 public:
  CoreCastService(CastWebService* web_service,
                  RuntimeApplicationDispatcher& app_dispatcher)
      : CastServiceSimple(web_service), app_dispatcher_(app_dispatcher) {}

  // CastServiceSimple overrides:
  void StartInternal() override {
    if (!app_dispatcher_->Start().ok()) {
      base::Process::TerminateCurrentProcessImmediately(1);
    }
  }

  void StopInternal() override { app_dispatcher_->Stop(); }

 private:
  base::raw_ref<RuntimeApplicationDispatcher> app_dispatcher_;
};

// Implementation of cast_receiver::ApplicationClient.
class CoreApplicationClient : public cast_receiver::ApplicationClient {
 public:
  explicit CoreApplicationClient(
      CastRuntimeContentBrowserClient& browser_client)
      : browser_client_(browser_client) {}

 private:
  // cast_receiver::ApplicationClient overrides:
  NetworkContextGetter GetNetworkContextGetter() override {
    return browser_client_->GetNetworkContextGetter();
  }

  base::raw_ref<CastRuntimeContentBrowserClient> browser_client_;
};

}  // namespace

CastRuntimeContentBrowserClient::CastRuntimeContentBrowserClient(
    CastFeatureListCreator* feature_list_creator)
    : shell::CastContentBrowserClient(feature_list_creator),
      application_client_(std::make_unique<CoreApplicationClient>(*this)) {
  application_client_->AddStreamingResolutionObserver(
      &application_client_observers_);
  application_client_->AddApplicationStateObserver(
      &application_client_observers_);
}

CastRuntimeContentBrowserClient::~CastRuntimeContentBrowserClient() {
  application_client_->RemoveStreamingResolutionObserver(
      &application_client_observers_);
  application_client_->RemoveApplicationStateObserver(
      &application_client_observers_);
}

std::unique_ptr<CastService> CastRuntimeContentBrowserClient::CreateCastService(
    content::BrowserContext* browser_context,
    CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
    PrefService* pref_service,
    media::VideoPlaneController* video_plane_controller,
    CastWindowManager* window_manager,
    CastWebService* web_service,
    DisplaySettingsManager* display_settings_manager) {
  application_client_observers_.SetVideoPlaneController(video_plane_controller);

  InitializeCoreComponents(web_service);

  // Unretained() is safe here because this instance will outlive CastService.
  return std::make_unique<CoreCastService>(web_service, *app_dispatcher_);
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
    const char* path[1] = {switches::kLogFile};
    command_line->CopySwitchesFrom(*browser_command_line, path, size_t{1});
  }
}

bool CastRuntimeContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return origin.host() == kCastWebUIHomeHost;
}

bool CastRuntimeContentBrowserClient::IsBufferingEnabled() {
  return application_client_observers_.IsBufferingEnabled();
}

void CastRuntimeContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  application_client_->OnWebContentsCreated(web_contents);
}

CastRuntimeContentBrowserClient::ApplicationClientObservers::
    ~ApplicationClientObservers() = default;

void CastRuntimeContentBrowserClient::ApplicationClientObservers::
    SetVideoPlaneController(
        media::VideoPlaneController* video_plane_controller) {
  video_plane_controller_ = video_plane_controller;
}

bool CastRuntimeContentBrowserClient::ApplicationClientObservers::
    IsBufferingEnabled() const {
  return is_buffering_enabled_.load();
}

void CastRuntimeContentBrowserClient::ApplicationClientObservers::
    OnForegroundApplicationChanged(cast_receiver::RuntimeApplication* app) {
  bool enabled = true;
  // Buffering must be disabled for streaming applications.
  if (app && app->IsStreamingApplication()) {
    enabled = false;
  }

  is_buffering_enabled_.store(enabled);
  DLOG(INFO) << "Buffering is " << (enabled ? "enabled" : "disabled");
}

void CastRuntimeContentBrowserClient::ApplicationClientObservers::
    OnStreamingResolutionChanged(
        const gfx::Rect& size,
        const ::media::VideoTransformation& transformation) {
  if (video_plane_controller_) {
    video_plane_controller_->SetGeometryFromMediaType(size, transformation);
  }
}

cast_receiver::ApplicationClient::NetworkContextGetter
CastRuntimeContentBrowserClient::GetNetworkContextGetter() {
  // Unretained() is safe here because this instance will outlive any
  // application that would call it.
  return base::BindRepeating(
      &CastRuntimeContentBrowserClient::GetSystemNetworkContext,
      base::Unretained(this));
}

void CastRuntimeContentBrowserClient::InitializeCoreComponents(
    CastWebService* web_service) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string runtime_id =
      command_line->GetSwitchValueASCII(cast::core::kCastCoreRuntimeIdSwitch);
  std::string runtime_service_path =
      command_line->GetSwitchValueASCII(cast::core::kRuntimeServicePathSwitch);

  app_dispatcher_ = std::make_unique<RuntimeServiceImpl>(
      *application_client_, *web_service, runtime_id, runtime_service_path);
}

}  // namespace chromecast

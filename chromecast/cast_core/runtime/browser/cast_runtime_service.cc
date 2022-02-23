// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_service.h"

#include "base/command_line.h"
#include "base/process/process.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/metrics/cast_event_builder_simple.h"

namespace chromecast {
namespace {

const char kCastCoreRuntimeIdSwitch[] = "cast-core-runtime-id";
const char kRuntimeServicePathSwitch[] = "runtime-service-path";

}  // namespace

CastRuntimeService::CastRuntimeService(
    CastWebService* web_service,
    NetworkContextGetter network_context_getter,
    media::VideoPlaneController* video_plane_controller,
    RuntimeApplicationWatcher* application_watcher)
    : video_plane_controller_(video_plane_controller),
      app_dispatcher_(web_service,
                      this,
                      std::move(network_context_getter),
                      video_plane_controller_,
                      application_watcher) {
  DCHECK(video_plane_controller_);
}

CastRuntimeService::~CastRuntimeService() = default;

void CastRuntimeService::InitializeInternal() {}

void CastRuntimeService::FinalizeInternal() {}

void CastRuntimeService::StartInternal() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string runtime_id =
      command_line->GetSwitchValueASCII(kCastCoreRuntimeIdSwitch);
  std::string runtime_service_path =
      command_line->GetSwitchValueASCII(kRuntimeServicePathSwitch);
  if (!app_dispatcher_.Start(runtime_id, runtime_service_path)) {
    base::Process::TerminateCurrentProcessImmediately(1);
  }
}

void CastRuntimeService::StopInternal() {
  app_dispatcher_.Stop();
}

std::unique_ptr<CastEventBuilder> CastRuntimeService::CreateEventBuilder() {
  return std::make_unique<CastEventBuilderSimple>();
}

const std::string& CastRuntimeService::GetAudioChannelEndpoint() {
  return app_dispatcher_.GetCastMediaServiceEndpoint();
}

WebCryptoServer* CastRuntimeService::GetWebCryptoServer() {
  return nullptr;
}

receiver::MediaManager* CastRuntimeService::GetMediaManager() {
  return nullptr;
}

}  // namespace chromecast

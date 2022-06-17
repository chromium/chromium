// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/core_browser_cast_service.h"

#include "base/command_line.h"
#include "base/process/process.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "chromecast/metrics/cast_event_builder_simple.h"

namespace chromecast {

CoreBrowserCastService::CoreBrowserCastService(
    CastWebService* web_service,
    NetworkContextGetter network_context_getter,
    media::VideoPlaneController* video_plane_controller)
    : app_dispatcher_(web_service,
                      this,
                      std::move(network_context_getter),
                      video_plane_controller) {}

void CoreBrowserCastService::InitializeInternal() {}

void CoreBrowserCastService::FinalizeInternal() {}

void CoreBrowserCastService::StartInternal() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string runtime_id =
      command_line->GetSwitchValueASCII(cast::core::kCastCoreRuntimeIdSwitch);
  std::string runtime_service_path =
      command_line->GetSwitchValueASCII(cast::core::kRuntimeServicePathSwitch);
  if (!app_dispatcher_.Start(runtime_id, runtime_service_path)) {
    base::Process::TerminateCurrentProcessImmediately(1);
  }
}

void CoreBrowserCastService::StopInternal() {
  app_dispatcher_.Stop();
}

std::unique_ptr<CastEventBuilder> CoreBrowserCastService::CreateEventBuilder() {
  return std::make_unique<CastEventBuilderSimple>();
}

const std::string& CoreBrowserCastService::GetAudioChannelEndpoint() {
  return app_dispatcher_.GetCastMediaServiceEndpoint();
}

WebCryptoServer* CoreBrowserCastService::GetWebCryptoServer() {
  return nullptr;
}

receiver::MediaManager* CoreBrowserCastService::GetMediaManager() {
  return nullptr;
}

}  // namespace chromecast

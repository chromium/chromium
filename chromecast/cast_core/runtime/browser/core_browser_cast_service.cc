// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/core_browser_cast_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/process/process.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher_platform.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher_platform_grpc.h"
#include "chromecast/metrics/cast_event_builder_simple.h"

namespace chromecast {
namespace {

std::unique_ptr<RuntimeApplicationDispatcherPlatform>
CreateApplicationDispatcherPlatform(
    CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
    RuntimeApplicationDispatcherPlatform::Client& client,
    CastWebService* web_service) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string runtime_id =
      command_line->GetSwitchValueASCII(cast::core::kCastCoreRuntimeIdSwitch);
  std::string runtime_service_path =
      command_line->GetSwitchValueASCII(cast::core::kRuntimeServicePathSwitch);

  return std::make_unique<RuntimeApplicationDispatcherPlatformGrpc>(
      client, web_service, event_builder_factory, runtime_id,
      runtime_service_path);
}

}  // namespace

CoreBrowserCastService::CoreBrowserCastService(
    CastWebService* web_service,
    cast_receiver::ApplicationClient& application_client)
    : app_dispatcher_(
          base::BindOnce(&CreateApplicationDispatcherPlatform, this),
          web_service,
          application_client) {}

CoreBrowserCastService::~CoreBrowserCastService() = default;

void CoreBrowserCastService::InitializeInternal() {}

void CoreBrowserCastService::FinalizeInternal() {}

void CoreBrowserCastService::StartInternal() {
  if (!app_dispatcher_.Start()) {
    base::Process::TerminateCurrentProcessImmediately(1);
  }
}

void CoreBrowserCastService::StopInternal() {
  app_dispatcher_.Stop();
}

std::unique_ptr<CastEventBuilder> CoreBrowserCastService::CreateEventBuilder() {
  return std::make_unique<CastEventBuilderSimple>();
}

WebCryptoServer* CoreBrowserCastService::GetWebCryptoServer() {
  return nullptr;
}

receiver::MediaManager* CoreBrowserCastService::GetMediaManager() {
  return nullptr;
}

}  // namespace chromecast

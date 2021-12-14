// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_service_impl.h"

#include "base/command_line.h"
#include "base/process/process.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/metrics/cast_event_builder_simple.h"

namespace chromecast {
namespace {

const char kCastCoreRuntimeIdSwitch[] = "cast-core-runtime-id";
const char kRuntimeServicePathSwitch[] = "runtime-service-path";

}  // namespace

CastRuntimeServiceImpl::CastRuntimeServiceImpl(
    CastWebService* web_service,
    NetworkContextGetter network_context_getter)
    : web_service_(web_service),
      app_dispatcher_(web_service, this, std::move(network_context_getter)) {}

CastRuntimeServiceImpl::~CastRuntimeServiceImpl() = default;

void CastRuntimeServiceImpl::InitializeInternal() {}

void CastRuntimeServiceImpl::FinalizeInternal() {}

void CastRuntimeServiceImpl::StartInternal() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string runtime_id =
      command_line->GetSwitchValueASCII(kCastCoreRuntimeIdSwitch);
  std::string runtime_service_path =
      command_line->GetSwitchValueASCII(kRuntimeServicePathSwitch);
  if (!app_dispatcher_.Start(runtime_id, runtime_service_path)) {
    base::Process::TerminateCurrentProcessImmediately(1);
  }
}

void CastRuntimeServiceImpl::StopInternal() {
  app_dispatcher_.Stop();
}

std::unique_ptr<CastEventBuilder> CastRuntimeServiceImpl::CreateEventBuilder() {
  return std::make_unique<CastEventBuilderSimple>();
}

const std::string& CastRuntimeServiceImpl::GetAudioChannelEndpoint() {
  return app_dispatcher_.GetCastMediaServiceGrpcEndpoint();
}

WebCryptoServer* CastRuntimeServiceImpl::GetWebCryptoServer() {
  return nullptr;
}

receiver::MediaManager* CastRuntimeServiceImpl::GetMediaManager() {
  return nullptr;
}

CastWebService* CastRuntimeServiceImpl::GetCastWebService() {
  return web_service_;
}

RuntimeApplication* CastRuntimeServiceImpl::GetRuntimeApplication() {
  return app_dispatcher_.GetRuntimeApplication();
}

}  // namespace chromecast

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

// static
CastRuntimeService* CastRuntimeService::GetInstance() {
  DCHECK(shell::CastBrowserProcess::GetInstance());
  auto* cast_service = shell::CastBrowserProcess::GetInstance()->cast_service();
  DCHECK(cast_service);
  return static_cast<CastRuntimeService*>(cast_service);
}

CastRuntimeServiceImpl::CastRuntimeServiceImpl(
    content::BrowserContext* browser_context,
    CastWindowManager* window_manager,
    NetworkContextGetter network_context_getter)
    : network_context_getter_(std::move(network_context_getter)),
      runtime_service_(browser_context, window_manager, this) {}

CastRuntimeServiceImpl::~CastRuntimeServiceImpl() = default;

void CastRuntimeServiceImpl::StartInternal() {
  CastRuntimeService::StartInternal();

  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string runtime_id =
      command_line->GetSwitchValueASCII(kCastCoreRuntimeIdSwitch);
  std::string runtime_service_path =
      command_line->GetSwitchValueASCII(kRuntimeServicePathSwitch);
  if (!runtime_service_.Start(runtime_id, runtime_service_path)) {
    base::Process::TerminateCurrentProcessImmediately(1);
  }
}

void CastRuntimeServiceImpl::StopInternal() {
  CastRuntimeService::StopInternal();

  runtime_service_.Stop();
}

std::unique_ptr<CastEventBuilder> CastRuntimeServiceImpl::CreateEventBuilder() {
  return std::make_unique<CastEventBuilderSimple>();
}

const std::string& CastRuntimeServiceImpl::GetAudioChannelEndpoint() {
  return runtime_service_.GetCastMediaServiceGrpcEndpoint();
}

}  // namespace chromecast

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_service.h"
#include "chromecast/chromecast_buildflags.h"

#if BUILDFLAG(ENABLE_CAST_MEDIA_RUNTIME)
#include "chromecast/browser/cast_browser_process.h"  // nogncheck
#else  // BUILDFLAG(ENABLE_CAST_MEDIA_RUNTIME)
#include "base/no_destructor.h"
#endif  // BUILDFLAG(ENABLE_CAST_MEDIA_RUNTIME)

namespace chromecast {
namespace {

static std::string kFakeAudioChannelEndpoint = "";

}  // namespace

// static
CastRuntimeService* CastRuntimeService::GetInstance() {
#if BUILDFLAG(ENABLE_CAST_MEDIA_RUNTIME)
  DCHECK(shell::CastBrowserProcess::GetInstance());
  auto* cast_service = shell::CastBrowserProcess::GetInstance()->cast_service();
  DCHECK(cast_service);
  return static_cast<CastRuntimeService*>(cast_service);
#else
  // TODO(b/186668532): Instead use the CastService singleton instead of
  // creating a new one with NoDestructor.
  static base::NoDestructor<CastRuntimeService> g_instance;
  return g_instance.get();
#endif  // BUILDFLAG(ENABLE_CAST_MEDIA_RUNTIME)
}

CastRuntimeService::CastRuntimeService() = default;

CastRuntimeService::~CastRuntimeService() = default;

WebCryptoServer* CastRuntimeService::GetWebCryptoServer() {
  return nullptr;
}

receiver::MediaManager* CastRuntimeService::GetMediaManager() {
  return nullptr;
}

void CastRuntimeService::InitializeInternal() {}

void CastRuntimeService::FinalizeInternal() {}

void CastRuntimeService::StartInternal() {}

void CastRuntimeService::StopInternal() {}

const std::string& CastRuntimeService::GetAudioChannelEndpoint() {
  return kFakeAudioChannelEndpoint;
}

}  // namespace chromecast

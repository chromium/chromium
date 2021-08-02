// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_service.h"

namespace chromecast {
namespace {

static std::string kFakeAudioChannelEndpoint = "";

}  // namespace

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

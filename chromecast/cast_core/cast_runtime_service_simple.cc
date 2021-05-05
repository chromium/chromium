// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "chromecast/cast_core/cast_runtime_service.h"

namespace chromecast {
namespace {

class CastRuntimeServiceSimple : public CastRuntimeService {
 public:
  CastRuntimeServiceSimple() = default;
  ~CastRuntimeServiceSimple() override = default;

 private:
  // CastRuntimeService overrides.
  WebCryptoServer* GetWebCryptoServer() override { return nullptr; }
  receiver::MediaManager* GetMediaManager() override { return nullptr; }

  // CastService overrides.
  void InitializeInternal() override {}
  void FinalizeInternal() override {}
  void StartInternal() override {}
  void StopInternal() override {}

  // CastRuntimeAudioChannelEndpointManager overrides.
  const std::string& GetAudioChannelEndpoint() override { return endpoint_; }

  const std::string endpoint_ = "";
};

}  // namespace

// static
CastRuntimeService* CastRuntimeService::GetInstance() {
  // TODO(b/186668532): Instead use the CastService singleton instead of
  // creating a new one with NoDestructor.
  static base::NoDestructor<CastRuntimeServiceSimple> g_instance;
  return g_instance.get();
}

}  // namespace chromecast

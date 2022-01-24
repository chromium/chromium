// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_IMPL_H_
#define CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_IMPL_H_

#include <memory>

#include "chromecast/cast_core/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/cast_runtime_service.h"
#include "chromecast/cast_core/runtime_application_dispatcher.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromecast {

class CastWindowManager;

class CastEventBuilder;

// This interface is to be used for building the Cast Runtime Service and act as
// the border between shared Chromium code and the specifics of that
// implementation.
class CastRuntimeServiceImpl
    : public CastRuntimeService,
      public CastRuntimeMetricsRecorder::EventBuilderFactory {
 public:
  CastRuntimeServiceImpl(content::BrowserContext* browser_context,
                         CastWindowManager* window_manager,
                         NetworkContextGetter network_context_getter);
  ~CastRuntimeServiceImpl() override;

  // CastService overrides.
  void StartInternal() override;
  void StopInternal() override;
  const std::string& GetAudioChannelEndpoint() override;
  CastWebService* GetCastWebService() override;

 protected:
  // CastRuntimeMetricsRecorder::EventBuilderFactory overrides.
  std::unique_ptr<CastEventBuilder> CreateEventBuilder() override;

 private:
  RuntimeApplicationDispatcher app_dispatcher_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_IMPL_H_

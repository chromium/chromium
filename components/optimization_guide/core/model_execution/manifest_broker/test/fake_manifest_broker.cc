// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"

#include "base/test/test_future.h"
#include "base/trace_event/trace_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

FakeManifestBroker::FakeManifestBroker() = default;
FakeManifestBroker::~FakeManifestBroker() = default;

void FakeManifestBroker::Startup() {
  if (!manifest_broker_state_) {
    manifest_broker_state_ = std::make_unique<ManifestBrokerState>(
        local_state_.local_state(), component_state_.CreateDelegate(),
        launcher_.LaunchFn(), &component_update_service_);
  }
  if (!model_broker_client_) {
    model_broker_client_ = std::make_unique<ModelBrokerClient>(
        manifest_broker_state_->BindAndPassRemoteBroker(), nullptr);
  }
}

void FakeManifestBroker::SimulateShutdown() {
  model_broker_client_.reset();
  manifest_broker_state_.reset();
  component_state_.SimulateRestart();
}

ModelBrokerClient::CreateSessionResult FakeManifestBroker::CreateSession() {
  TRACE_EVENT("optimization_guide", "CreateSession");
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  model_broker_client_->CreateSession("test", SessionConfigParams{},
                                      session_future.GetCallback());
  return session_future.Take();
}

bool FakeManifestBroker::WarmupPrefsAndAssets() {
  TRACE_EVENT("optimization_guide", "WarmupPrefsAndAssets");
  Startup();
  if (!CreateSession()) {
    return false;
  }
  SimulateShutdown();
  Startup();
  return true;
}

}  // namespace optimization_guide

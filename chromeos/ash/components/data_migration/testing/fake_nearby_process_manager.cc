// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/testing/fake_nearby_process_manager.h"

#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace data_migration {
namespace {

using NearbyConnectionsMojom = ::nearby::connections::mojom::NearbyConnections;

class FakeNearbyProcessReference
    : public ash::nearby::NearbyProcessManager::NearbyProcessReference {
 public:
  explicit FakeNearbyProcessReference(
      mojo::SharedRemote<NearbyConnectionsMojom> nearby_connections)
      : nearby_connections_(std::move(nearby_connections)) {}
  FakeNearbyProcessReference(const FakeNearbyProcessReference&) = delete;
  FakeNearbyProcessReference& operator=(const FakeNearbyProcessReference&) =
      delete;
  ~FakeNearbyProcessReference() override = default;

  const mojo::SharedRemote<NearbyConnectionsMojom>& GetNearbyConnections()
      const override {
    return nearby_connections_;
  }

  // None of the below are used for data migration.
  const mojo::SharedRemote<ash::nearby::presence::mojom::NearbyPresence>&
  GetNearbyPresence() const override {
    NOTIMPLEMENTED();
    return nearby_presence_;
  }

  const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
  GetNearbySharingDecoder() const override {
    NOTIMPLEMENTED();
    return nearby_sharing_decoder_;
  }

  const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>&
  GetQuickStartDecoder() const override {
    NOTIMPLEMENTED();
    return quick_start_decoder_;
  }

 private:
  const mojo::SharedRemote<NearbyConnectionsMojom> nearby_connections_;
  const mojo::SharedRemote<ash::nearby::presence::mojom::NearbyPresence>
      nearby_presence_;
  const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>
      nearby_sharing_decoder_;
  const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>
      quick_start_decoder_;
};

}  // namespace

FakeNearbyProcessManager::FakeNearbyProcessManager(
    std::string_view remote_endpoint_id)
    : fake_nearby_connections_(std::move(remote_endpoint_id)),
      receiver_(&fake_nearby_connections_) {
  InitializeProcess();
}

FakeNearbyProcessManager::~FakeNearbyProcessManager() {
  if (remote_.is_bound()) {
    // Flushes any pending NearbyConnections mojo calls that the test may have
    // made. Since the mojo call made below uses the same message pipe as that
    // used during tests and mojo guarantees the ordering of calls made on the
    // same message pipe, any pending mojo calls made during the test are
    // guaranteed to be completed before the test exits. Failure to do so can
    // result in memory leaks reported by LSAN; mojo leaks memory internally if
    // the request's message loop is destroyed before a mojo call completes.
    base::RunLoop run_loop;
    remote_->StopAllEndpoints(
        kServiceId,
        base::BindLambdaForTesting(
            [&run_loop](FakeNearbyConnections::Status) { run_loop.Quit(); }));
    run_loop.Run();
  }
}

std::unique_ptr<ash::nearby::NearbyProcessManager::NearbyProcessReference>
FakeNearbyProcessManager::GetNearbyProcessReference(
    NearbyProcessStoppedCallback on_process_stopped_callback) {
  if (!remote_.is_bound() || !receiver_.is_bound()) {
    InitializeProcess();
  }
  return std::make_unique<FakeNearbyProcessReference>(remote_);
}

void FakeNearbyProcessManager::ShutDownProcess() {
  receiver_.reset();
  remote_.reset();
}

void FakeNearbyProcessManager::InitializeProcess() {
  ShutDownProcess();
  remote_.Bind(receiver_.BindNewPipeAndPassRemote(),
               base::SequencedTaskRunner::GetCurrentDefault());
}

}  // namespace data_migration

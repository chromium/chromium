// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/fake_ash_event_reporter.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::cros_healthd {

FakeAshEventReporter::FakeAshEventReporter() {
  CHECK(mojo_service_manager::IsServiceManagerBound());
  mojo_service_manager::GetServiceManagerProxy()->Register(
      chromeos::mojo_services::kCrosHealthdAshEventReporter,
      provider_receiver_.BindNewPipeAndPassRemote());
}

FakeAshEventReporter::~FakeAshEventReporter() = default;

ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr
FakeAshEventReporter::WaitKeyboardDiagnosticEvent() {
  keyboard_event_run_loop_->Run();
  keyboard_event_run_loop_ = std::make_unique<base::RunLoop>();
  return std::move(keyboard_diagnostic_event_);
}

void FakeAshEventReporter::SendKeyboardDiagnosticEvent(
    ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr info) {
  keyboard_diagnostic_event_ = std::move(info);
  keyboard_event_run_loop_->Quit();
}

void FakeAshEventReporter::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  service_receiver_set_.Add(
      this,
      mojo::PendingReceiver<mojom::AshEventReporter>(std::move(receiver)));
}

}  // namespace ash::cros_healthd

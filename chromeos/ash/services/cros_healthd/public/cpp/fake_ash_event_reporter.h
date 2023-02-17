// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ASH_EVENT_REPORTER_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ASH_EVENT_REPORTER_H_

#include <memory>

#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_event_reporters.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cros_healthd {

class FakeAshEventReporter final
    : public mojom::AshEventReporter,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  FakeAshEventReporter();
  FakeAshEventReporter(const FakeAshEventReporter&) = delete;
  FakeAshEventReporter& operator=(const FakeAshEventReporter&) = delete;
  ~FakeAshEventReporter() override;

  ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr
  WaitKeyboardDiagnosticEvent();

  // mojom::AshEventReporter overrides.
  void SendKeyboardDiagnosticEvent(
      ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr info) override;

  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

 private:
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};
  mojo::ReceiverSet<mojom::AshEventReporter> service_receiver_set_;

  std::unique_ptr<base::RunLoop> keyboard_event_run_loop_{
      std::make_unique<base::RunLoop>()};

  ash::diagnostics::mojom::KeyboardDiagnosticEventInfoPtr
      keyboard_diagnostic_event_;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ASH_EVENT_REPORTER_H_

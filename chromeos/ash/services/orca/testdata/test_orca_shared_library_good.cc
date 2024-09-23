// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/simple_thread.h"
#include "chromeos/ash/services/orca/public/cpp/orca_entry.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/c/system/thunks.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

class FakeOrcaService : public ash::orca::mojom::OrcaService {
 public:
  explicit FakeOrcaService(OrcaLogger* logger) : logger_(logger) {}

  void BindEditor(
      mojo::PendingAssociatedRemote<ash::orca::mojom::SystemActuator>
          text_actuator,
      mojo::PendingAssociatedRemote<ash::orca::mojom::TextQueryProvider>
          text_query_provider,
      mojo::PendingAssociatedReceiver<ash::orca::mojom::EditorClientConnector>
          client_connector,
      mojo::PendingAssociatedReceiver<ash::orca::mojom::EditorEventSink>
          event_sink,
      ash::orca::mojom::EditorConfigPtr editor_config) override {
    logger_->log(logger_, OrcaLogSeverity::ORCA_LOG_SEVERITY_WARNING,
                 "Success");
  }

 private:
  raw_ptr<OrcaLogger> logger_;
};

class OrcaServiceThread : public base::SimpleThread {
 public:
  OrcaServiceThread(uint32_t receiver_handle, OrcaLogger* logger)
      : base::SimpleThread("OrcaServiceThread"),
        receiver_handle_(receiver_handle),
        logger_(logger) {}

 private:
  // SimpleThread:
  void Run() override {
    base::SingleThreadTaskExecutor executor;

    FakeOrcaService fake_orca_service(logger_);
    mojo::Receiver<ash::orca::mojom::OrcaService> receiver(
        &fake_orca_service,
        mojo::PendingReceiver<ash::orca::mojom::OrcaService>(
            mojo::ScopedMessagePipeHandle(
                mojo::MessagePipeHandle(receiver_handle_))));

    base::RunLoop run_loop;
    receiver.set_disconnect_handler(base::BindOnce(
        [](base::RepeatingClosure quit_closure) { quit_closure.Run(); },
        run_loop.QuitClosure()));

    // Blocks this thread until `run_loop.Quit()` is called
    // upon `receiver` disconnecting.
    run_loop.Run();
  }

  uint32_t receiver_handle_;
  raw_ptr<OrcaLogger> logger_;
};

OrcaServiceThread* g_orca_service_thread = nullptr;

}  // namespace

extern "C" {

OrcaBindServiceStatus __attribute__((visibility("default")))
OrcaBindService(const MojoSystemThunks2* mojo_thunks,
                const MojoSystemThunks* mojo_thunks_legacy,
                uint32_t receiver_handle,
                OrcaLogger* logger) {
  CHECK(mojo_thunks);
  MojoEmbedderSetSystemThunks(mojo_thunks);

  // Run the OrcaService in a separate thread so that it can have its own
  // RunLoop to listen for Mojo messages.
  g_orca_service_thread = new OrcaServiceThread(receiver_handle, logger);
  g_orca_service_thread->Start();

  return OrcaBindServiceStatus::ORCA_BIND_SERVICE_STATUS_OK;
}

void OrcaResetService() {
  if (!g_orca_service_thread) {
    return;
  }

  g_orca_service_thread->Join();
  delete g_orca_service_thread;
}
}

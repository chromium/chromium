// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/services_initializer_base.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"

namespace ash::libassistant {

ServicesInitializerBase::ServicesInitializerBase(
    const std::string& cq_thread_name,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : cq_thread_(cq_thread_name), main_task_runner_(main_task_runner) {
  base::Thread::Options options(base::MessagePumpType::IO,
                                0 /* default maximum stack size */);
  options.thread_type = base::ThreadType::kDefault;
  cq_thread_.StartWithOptions(std::move(options));
}

ServicesInitializerBase::~ServicesInitializerBase() {
  StopCQ();
}

void ServicesInitializerBase::RegisterServicesAndInitCQ(
    grpc::ServerBuilder* server_builder) {
  // Register all services.
  InitDrivers(server_builder);
  // Get hold of the completion queue used for async gRPC runtime.
  cq_ = server_builder->AddCompletionQueue();
}

void ServicesInitializerBase::StartCQ() {
  // Initialize completion queues for each service.
  for (auto* const driver : service_drivers_) {
    driver->StartCQ(cq_.get());
  }
  cq_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServicesInitializerBase::ScanCQInternal,
                                base::Unretained(this)));
}

void ServicesInitializerBase::StopCQ() {
  if (cq_) {
    // Make sure |cq_thread_| quits before draining CQ in order to prevent race.
    cq_->Shutdown();
    cq_thread_.Stop();
    // Drain CQ
    void* ignored_tag;
    bool ignored_ok;
    while (cq_->Next(&ignored_tag, &ignored_ok)) {
    }
    cq_ = nullptr;
  }

  // If |cq_| was not created, no need to explicitly stop |cq_thread_| since
  // Stop() will be called in its destructor.
}

// Runs on the cq polling thread.
void ServicesInitializerBase::ScanCQInternal() {
  // Poll the completion queue.
  while (true) {
    void* tag = nullptr;
    bool ok = false;
    if (!cq_->Next(&tag, &ok)) {
      // Completion queue is shutting down.
      DVLOG(3) << "Completion queue shutdown.";
      break;
    }
    if (!tag) {
      LOG(ERROR) << "Failed to fetch tag from completion queue.";
      continue;
    }
    auto* cb_ptr = static_cast<base::OnceCallback<void(bool)>*>(tag);
    // Use PostTask() to ensure callbacks are run on the same sequence on which
    // they are bound.
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(*cb_ptr), ok));
    delete cb_ptr;
  }
}

}  // namespace ash::libassistant

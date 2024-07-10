// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/dm_client.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/enterprise_companion_service_stub.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "chrome/enterprise_companion/lock.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/enterprise_companion/url_loader_factory_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_companion {

namespace {

// AppServer runs the EnterpriseCompanion Mojo IPC server process.
class AppServer : public App {
 public:
  AppServer() {
    net_thread_.StartWithOptions({base::MessagePumpType::IO, 0});
    url_loader_factory_provider_ =
        base::SequenceBound<URLLoaderFactoryProvider>(
            net_thread_.task_runner());
  }

  ~AppServer() override { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

 protected:
  void FirstTaskRun() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    lock_ = CreateScopedLock();
    if (!lock_) {
      Shutdown(EnterpriseCompanionStatus(ApplicationError::kCannotAcquireLock));
      return;
    }

    url_loader_factory_provider_
        .AsyncCall(&URLLoaderFactoryProvider::GetPendingURLLoaderFactory)
        .Then(base::BindOnce(&AppServer::OnUrlLoaderFactoryReceived,
                             weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::Thread net_thread_{"Network"};
  base::SequenceBound<URLLoaderFactoryProvider> url_loader_factory_provider_;
  std::unique_ptr<ScopedLock> lock_;
  std::unique_ptr<mojom::EnterpriseCompanion> stub_;

  void OnUrlLoaderFactoryReceived(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        network::SharedURLLoaderFactory::Create(
            std::move(pending_url_loader_factory));

    VLOG(1) << "Launching Chrome Enterprise Companion App";
    stub_ =
        CreateEnterpriseCompanionServiceStub(CreateEnterpriseCompanionService(
            CreateDMClient(
                GetDefaultCloudPolicyClientProvider(url_loader_factory)),
            CreateEventLoggerManager(
                CreateEventLogUploader(url_loader_factory)),
            base::BindOnce(&AppServer::Shutdown, weak_ptr_factory_.GetWeakPtr(),
                           EnterpriseCompanionStatus::Success())));
  }

  base::WeakPtrFactory<AppServer> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<App> CreateAppServer() {
  return std::make_unique<AppServer>();
}

}  // namespace enterprise_companion

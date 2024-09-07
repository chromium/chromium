// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
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

#if !BUILDFLAG(IS_MAC)
#include "base/threading/thread.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <atlsecurity.h>

#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/scoped_handle.h"
#endif

namespace enterprise_companion {

namespace {

#if BUILDFLAG(IS_WIN)
bool IsSystemProcess() {
  CAccessToken current_process_token;
  if (!current_process_token.GetProcessToken(TOKEN_QUERY,
                                             ::GetCurrentProcess())) {
    PLOG(ERROR) << "CAccessToken::GetProcessToken failed";
    return false;
  }

  CSid logon_sid;
  if (!current_process_token.GetUser(&logon_sid)) {
    PLOG(ERROR) << "CAccessToken::GetUser failed";
    return false;
  }

  return logon_sid == Sids::System();
}
#endif

// AppServer runs the EnterpriseCompanion Mojo IPC server process.
class AppServer : public App {
 public:
  AppServer() {
#if !BUILDFLAG(IS_MAC)
    net_thread_.StartWithOptions({base::MessagePumpType::IO, 0});
#endif
#if BUILDFLAG(IS_WIN)
    // Try to impersonate the logged-in user for the lifetime of the net thread.
    // Skip impersonation if the process is not running as the SYSTEM user,
    // which should only be true in tests.
    if (IsSystemProcess()) {
      net_thread_.task_runner()->PostTask(
          FROM_HERE, base::BindOnce([] {
            updater::HResultOr<updater::ScopedKernelHANDLE> token =
                updater::GetLoggedOnUserToken();
            VLOG_IF(2, !token.has_value())
                << __func__ << ": GetLoggedOnUserToken failed: " << std::hex
                << token.error();
            if (token.has_value()) {
              if (!::ImpersonateLoggedOnUser(token->get())) {
                PLOG(ERROR)
                    << "Failed to impersonate logged on user. Networking "
                       "may fail.";
              }
            }
          }));
    }
#endif
  }

  ~AppServer() override { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

 protected:
  void FirstTaskRun() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_WIN)
    if (!com_initializer_.Succeeded()) {
      VLOG(1) << "Failed to initialize COM";
      Shutdown(EnterpriseCompanionStatus(
          ApplicationError::kCOMInitializationFailed));
      return;
    }
#endif

    lock_ = CreateScopedLock();
    if (!lock_) {
      Shutdown(EnterpriseCompanionStatus(ApplicationError::kCannotAcquireLock));
      return;
    }

#if BUILDFLAG(IS_MAC)
    url_loader_factory_provider_ = CreateOutOfProcessNetWorker(base::BindOnce(
        &AppServer::Shutdown, weak_ptr_factory_.GetWeakPtr(),
        EnterpriseCompanionStatus(ApplicationError::kMojoConnectionFailed)));
    if (!url_loader_factory_provider_) {
      Shutdown(
          EnterpriseCompanionStatus(ApplicationError::kMojoConnectionFailed));
      return;
    }
#else
    base::SequenceBound<EventLoggerCookieHandler> event_logger_cookie_handler =
        CreateEventLoggerCookieHandler();
    if (!event_logger_cookie_handler) {
      LOG(WARNING) << "Failed to create EventLoggerCookieHandler, logging "
                      "cookies will not be transmitted or persisted.";
    }
    url_loader_factory_provider_ = CreateInProcessUrlLoaderFactoryProvider(
        net_thread_.task_runner(), std::move(event_logger_cookie_handler));
#endif

    url_loader_factory_provider_
        .AsyncCall(&URLLoaderFactoryProvider::GetPendingURLLoaderFactory)
        .Then(base::BindOnce(&AppServer::OnUrlLoaderFactoryReceived,
                             weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

#if !BUILDFLAG(IS_MAC)
  base::Thread net_thread_{"Network"};
#endif

#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};
#endif
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

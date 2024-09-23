// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "chrome/enterprise_companion/url_loader_factory_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace enterprise_companion {

namespace {

constexpr uid_t kNobodyUid = -2;
constexpr gid_t kNobodyGid = -2;

// AppNetWorker runs networking tasks for the companion app in a dedicated
// process.
class AppNetWorker : public App {
 public:
  AppNetWorker() {
    net_thread_.StartWithOptions({base::MessagePumpType::IO, 0});
  }

  ~AppNetWorker() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void FirstTaskRun() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // The cookie handler is created before reducing privilege, as opening the
    // cookie file requires root.
    base::SequenceBound<EventLoggerCookieHandler> event_logger_cookie_handler =
        CreateEventLoggerCookieHandler();
    if (!event_logger_cookie_handler) {
      LOG(WARNING) << "Failed to create EventLoggerCookieHandler, logging "
                      "cookies will not be transmitted or persisted.";
    }

    // If running as root, drop down to "nobody".
    if (getuid() == 0) {
      if (setgid(kNobodyGid)) {
        PLOG(ERROR) << "Failed to set gid " << kNobodyGid;
        Shutdown(EnterpriseCompanionStatus::FromPosixErrno(errno));
        return;
      }

      if (setuid(kNobodyUid)) {
        PLOG(ERROR) << "Failed to set uid " << kNobodyUid;
        Shutdown(EnterpriseCompanionStatus::FromPosixErrno(errno));
        return;
      }
    }

    mojo::PlatformChannelEndpoint endpoint =
        mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            *base::CommandLine::ForCurrentProcess());
    if (!endpoint.is_valid()) {
      Shutdown(
          EnterpriseCompanionStatus(ApplicationError::kMojoConnectionFailed));
      return;
    }

    mojo::ScopedMessagePipeHandle pipe =
        mojo::IncomingInvitation::AcceptIsolated(std::move(endpoint));
    if (!pipe->is_valid()) {
      Shutdown(
          EnterpriseCompanionStatus(ApplicationError::kMojoConnectionFailed));
      return;
    }

    url_loader_factory_provider_ = CreateInProcessUrlLoaderFactoryProvider(
        net_thread_.task_runner(), std::move(event_logger_cookie_handler),
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>(
            std::move(pipe)),
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &AppNetWorker::Shutdown, weak_ptr_factory_.GetWeakPtr(),
            EnterpriseCompanionStatus(
                ApplicationError::kMojoConnectionFailed))));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::Thread net_thread_{"Network"};
  base::SequenceBound<URLLoaderFactoryProvider> url_loader_factory_provider_;
  base::WeakPtrFactory<AppNetWorker> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<App> CreateAppNetWorker() {
  return std::make_unique<AppNetWorker>();
}

}  // namespace enterprise_companion

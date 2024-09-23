// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/enterprise_companion/dm_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/event_logger.h"

namespace enterprise_companion {

class EnterpriseCompanionServiceImpl : public EnterpriseCompanionService {
 public:
  EnterpriseCompanionServiceImpl(
      std::unique_ptr<DMClient> dm_client,
      base::OnceClosure shutdown_callback,
      std::unique_ptr<EventLoggerManager> event_logger_manager)
      : dm_client_(std::move(dm_client)),
        shutdown_callback_(std::move(shutdown_callback)),
        event_logger_manager_(std::move(event_logger_manager)) {}
  ~EnterpriseCompanionServiceImpl() override = default;

  // Overrides for EnterpriseCompanionService.
  void Shutdown(base::OnceClosure callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;

    std::move(callback).Run();
    if (shutdown_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(shutdown_callback_));
    }
  }

  void FetchPolicies(StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;

    scoped_refptr<EventLogger> event_logger =
        event_logger_manager_->CreateEventLogger();
    dm_client_->RegisterPolicyAgent(
        event_logger,
        base::BindOnce(&EnterpriseCompanionServiceImpl::OnRegistrationCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       event_logger));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<DMClient> dm_client_;
  base::OnceClosure shutdown_callback_;
  std::unique_ptr<EventLoggerManager> event_logger_manager_;

  void OnRegistrationCompleted(
      StatusCallback policy_fetch_callback,
      scoped_refptr<EventLogger> event_logger,
      const EnterpriseCompanionStatus& device_registration_status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!device_registration_status.ok()) {
      std::move(policy_fetch_callback).Run(device_registration_status);
    } else {
      dm_client_->FetchPolicies(event_logger, std::move(policy_fetch_callback));
    }
  }

  base::WeakPtrFactory<EnterpriseCompanionServiceImpl> weak_ptr_factory_{this};
};

std::unique_ptr<EnterpriseCompanionService> CreateEnterpriseCompanionService(
    std::unique_ptr<DMClient> dm_client,
    std::unique_ptr<EventLoggerManager> event_logger_manager,
    base::OnceClosure shutdown_callback) {
  return std::make_unique<EnterpriseCompanionServiceImpl>(
      std::move(dm_client), std::move(shutdown_callback),
      std::move(event_logger_manager));
}

}  // namespace enterprise_companion

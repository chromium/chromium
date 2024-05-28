// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_service.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace enterprise_companion {

class EnterpriseCompanionServiceImpl : public EnterpriseCompanionService {
 public:
  explicit EnterpriseCompanionServiceImpl(base::OnceClosure shutdown_callback)
      : shutdown_callback_(std::move(shutdown_callback)) {}
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

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::OnceClosure shutdown_callback_;
};

std::unique_ptr<EnterpriseCompanionService> CreateEnterpriseCompanionService(
    base::OnceClosure shutdown_callback) {
  return std::make_unique<EnterpriseCompanionServiceImpl>(
      std::move(shutdown_callback));
}

}  // namespace enterprise_companion

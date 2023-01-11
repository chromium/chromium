// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module_delegate.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"

namespace reporting {

HealthModuleDelegate::HealthModuleDelegate() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

HealthModuleDelegate::~HealthModuleDelegate() {
  // Because of weak ptr factory, must be on the same sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HealthModuleDelegate::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const Status init_result = DoInit();
  if (!init_result.ok()) {
    LOG(ERROR) << "Health module failed to initialize, status=" << init_result;
    return;
  }
  initialized_ = true;
}

bool HealthModuleDelegate::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initialized_;
}

void HealthModuleDelegate::GetERPHealthData(HealthCallback cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoGetERPHealthData(std::move(cb));
}

void HealthModuleDelegate::PostHealthRecord(HealthDataHistory record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoPostHealthRecord(std::move(record));
}

base::WeakPtr<HealthModuleDelegate> HealthModuleDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace reporting

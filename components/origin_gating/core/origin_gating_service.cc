// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/memory/ptr_util.h"
#include "components/origin_gating/core/checker_id.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_configuration.h"

namespace origin_gating {

OriginGatingService::OriginGatingService() = default;

OriginGatingService::OriginGatingService(
    base::PassKey<OriginGatingServiceFactory>) {}

// static
std::unique_ptr<OriginGatingService> OriginGatingService::CreateForTesting() {
  return base::WrapUnique(new OriginGatingService());
}

OriginGatingService::~OriginGatingService() = default;

std::unique_ptr<OriginGatingRegistration>
OriginGatingService::CreateAndRegisterChecker(
    base::WeakPtr<OriginGatingChecker::Delegate> delegate,
    OriginGatingConfiguration config) {
  CHECK(!is_shutdown_);
  CheckerId id = id_generator_.GenerateNextId();
  checkers_.emplace(
      id, std::make_unique<OriginGatingChecker>(delegate, std::move(config)));
  return std::make_unique<OriginGatingRegistration>(
      base::PassKey<OriginGatingService>(), *this, id);
}

void OriginGatingService::UnregisterChecker(
    base::PassKey<OriginGatingRegistration>,
    CheckerId id) {
  CHECK(!id.is_null());
  if (is_shutdown_) {
    CHECK(checkers_.empty());
    return;
  }
  CHECK_EQ(checkers_.erase(id), 1u);
}

OriginGatingChecker* OriginGatingService::GetChecker(CheckerId id) const {
  return base::FindPtrOrNull(checkers_, id);
}

void OriginGatingService::Shutdown() {
  is_shutdown_ = true;
  checkers_.clear();
}

}  // namespace origin_gating

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/keyed_service/quick_pair_keyed_service.h"

#include <memory>

#include "chromeos/components/quick_pair/keyed_service/quick_pair_mediator.h"

namespace chromeos {
namespace quick_pair {

QuickPairKeyedService::QuickPairKeyedService(std::unique_ptr<Mediator> mediator)
    : mediator_(std::move(mediator)) {}

QuickPairKeyedService::~QuickPairKeyedService() = default;

void QuickPairKeyedService::Shutdown() {
  mediator_.reset();
}

}  // namespace quick_pair
}  // namespace chromeos

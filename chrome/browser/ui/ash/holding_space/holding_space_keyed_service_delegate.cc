// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace ash {

HoldingSpaceKeyedServiceDelegate::~HoldingSpaceKeyedServiceDelegate() = default;

HoldingSpaceKeyedServiceDelegate::HoldingSpaceKeyedServiceDelegate(
    HoldingSpaceModel* model)
    : model_(model) {
  holding_space_model_observer_.Add(model);
}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {}

}  // namespace ash

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_keyed_service.h"

LensKeyedService::LensKeyedService() = default;

LensKeyedService::~LensKeyedService() = default;

void LensKeyedService::IncrementActionChipShownCount() {
  action_chip_shown_count_ += 1;
}

int LensKeyedService::GetActionChipShownCount() {
  return action_chip_shown_count_;
}

void LensKeyedService::SetActionChipShownCount(int value) {
  action_chip_shown_count_ = value;
}

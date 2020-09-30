// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_ui_controller.h"
#include "base/check.h"

namespace chromeos {
namespace bloom {

// static
BloomUiController* BloomUiController::g_instance_ = nullptr;

// static
BloomUiController* BloomUiController::Get() {
  return g_instance_;
}

BloomUiController::BloomUiController() {
  DCHECK(g_instance_ == nullptr);
  g_instance_ = this;
}

BloomUiController::~BloomUiController() {
  g_instance_ = nullptr;
}

}  // namespace bloom
}  // namespace chromeos

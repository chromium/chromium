// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"

#include "base/check_op.h"

namespace chromeos {

namespace {

FloatControllerBase* g_instance = nullptr;

}  // namespace

FloatControllerBase::~FloatControllerBase() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FloatControllerBase* FloatControllerBase::Get() {
  return g_instance;
}

FloatControllerBase::FloatControllerBase() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

}  // namespace chromeos

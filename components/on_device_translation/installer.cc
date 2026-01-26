// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/installer.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace on_device_translation {

OnDeviceTranslationInstaller* g_instance = nullptr;

OnDeviceTranslationInstaller::OnDeviceTranslationInstaller() {
  // We only allow the instance to be overwritten if we are running in test
  // mode.
  if (g_instance) {
    CHECK_IS_TEST();
  } else {
    CHECK(!g_instance);
  }

  g_instance = this;
}

OnDeviceTranslationInstaller::~OnDeviceTranslationInstaller() = default;

// static
OnDeviceTranslationInstaller* OnDeviceTranslationInstaller::GetInstance() {
  return g_instance;
}

}  // namespace on_device_translation

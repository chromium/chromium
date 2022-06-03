// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/device_actions.h"

namespace chromeos {
namespace assistant {

namespace {

DeviceActions* g_instance = nullptr;

}  // namespace

// static
DeviceActions* DeviceActions::Get() {
  DCHECK(g_instance);
  return g_instance;
}

DeviceActions::DeviceActions() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

DeviceActions::~DeviceActions() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace assistant
}  // namespace chromeos

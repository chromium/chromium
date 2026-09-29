// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/media_device_salt/media_device_salt_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
MediaDeviceSaltServiceProvider* g_instance = nullptr;
}  // namespace

MediaDeviceSaltServiceProvider::MediaDeviceSaltServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

MediaDeviceSaltServiceProvider::~MediaDeviceSaltServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
MediaDeviceSaltServiceProvider& MediaDeviceSaltServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash

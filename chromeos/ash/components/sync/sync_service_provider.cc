// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync/sync_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
SyncServiceProvider* g_instance = nullptr;
}  // namespace

SyncServiceProvider::SyncServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

SyncServiceProvider::~SyncServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
SyncServiceProvider& SyncServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/desks_storage/desk_sync_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
DeskSyncServiceProvider* g_instance = nullptr;
}  // namespace

DeskSyncServiceProvider::DeskSyncServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

DeskSyncServiceProvider::~DeskSyncServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
DeskSyncServiceProvider& DeskSyncServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash

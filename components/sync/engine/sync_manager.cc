// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_manager.h"

namespace syncer {

SyncManager::Observer::~Observer() = default;

SyncManager::InitArgs::InitArgs()
    : enable_local_sync_backend(false),
      extensions_activity(nullptr),
      encryption_handler(nullptr),
      cancelation_signal(nullptr) {}

SyncManager::InitArgs::~InitArgs() = default;

SyncManager::SyncManager() = default;

SyncManager::~SyncManager() = default;

}  // namespace syncer

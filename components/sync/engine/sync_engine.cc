// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_engine.h"

#include "components/sync/engine/engine_components_factory.h"

namespace syncer {

SyncEngine::InitParams::InitParams() = default;
SyncEngine::InitParams::InitParams(InitParams&& other) = default;
SyncEngine::InitParams::~InitParams() = default;

SyncEngine::SyncEngine() = default;
SyncEngine::~SyncEngine() = default;

}  // namespace syncer

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"

namespace syncer {

NonBlockingTypeDebugInfoEmitter::NonBlockingTypeDebugInfoEmitter(ModelType type)
    : DataTypeDebugInfoEmitter(type) {}

NonBlockingTypeDebugInfoEmitter::~NonBlockingTypeDebugInfoEmitter() {}

}  // namespace syncer

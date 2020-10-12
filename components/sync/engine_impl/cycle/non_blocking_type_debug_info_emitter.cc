// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"

namespace syncer {

NonBlockingTypeDebugInfoEmitter::NonBlockingTypeDebugInfoEmitter(ModelType type)
    : DataTypeDebugInfoEmitter(type) {}

NonBlockingTypeDebugInfoEmitter::~NonBlockingTypeDebugInfoEmitter() {}

void NonBlockingTypeDebugInfoEmitter::EmitStatusCountersUpdate() {
  // TODO(gangwu): Allow driving emission of status counters from here. This is
  // tricky because we do not have access to ClientTagBasedModelTypeProcessor or
  // ModelTypeStore currently. This method is fairly redundant since counters
  // are also emitted from the UI thread, unclear how important this is.
}

}  // namespace syncer

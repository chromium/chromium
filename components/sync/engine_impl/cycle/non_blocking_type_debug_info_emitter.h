// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_NON_BLOCKING_TYPE_DEBUG_INFO_EMITTER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_NON_BLOCKING_TYPE_DEBUG_INFO_EMITTER_H_

#include "base/macros.h"
#include "components/sync/engine_impl/cycle/data_type_debug_info_emitter.h"

namespace syncer {

class NonBlockingTypeDebugInfoEmitter : public DataTypeDebugInfoEmitter {
 public:
  explicit NonBlockingTypeDebugInfoEmitter(ModelType type);

  ~NonBlockingTypeDebugInfoEmitter() override;

  // Triggers a status counters update to registered observers.
  void EmitStatusCountersUpdate() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonBlockingTypeDebugInfoEmitter);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_NON_BLOCKING_TYPE_DEBUG_INFO_EMITTER_H_

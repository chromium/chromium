// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_MOCK_DEBUG_INFO_GETTER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_MOCK_DEBUG_INFO_GETTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/engine_impl/cycle/debug_info_getter.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// A mock implementation of DebugInfoGetter to be used in tests. Events added by
// AddDebugEvent are accessible via DebugInfoGetter methods.
class MockDebugInfoGetter : public DebugInfoGetter {
 public:
  MockDebugInfoGetter();
  ~MockDebugInfoGetter() override;

  // DebugInfoGetter implementation.
  sync_pb::DebugInfo GetDebugInfo() const override;
  void ClearDebugInfo() override;

  void AddDebugEvent();

 private:
  sync_pb::DebugInfo debug_info_;

  DISALLOW_COPY_AND_ASSIGN(MockDebugInfoGetter);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_MOCK_DEBUG_INFO_GETTER_H_

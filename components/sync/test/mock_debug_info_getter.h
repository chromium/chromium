// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_DEBUG_INFO_GETTER_H_
#define COMPONENTS_SYNC_TEST_MOCK_DEBUG_INFO_GETTER_H_

#include "base/compiler_specific.h"
#include "components/sync/engine/cycle/debug_info_getter.h"
#include "components/sync/protocol/client_debug_info.pb.h"

namespace syncer {

// A mock implementation of DebugInfoGetter to be used in tests. Events added by
// AddDebugEvent are accessible via DebugInfoGetter methods.
class MockDebugInfoGetter : public DebugInfoGetter {
 public:
  MockDebugInfoGetter();

  MockDebugInfoGetter(const MockDebugInfoGetter&) = delete;
  MockDebugInfoGetter& operator=(const MockDebugInfoGetter&) = delete;

  ~MockDebugInfoGetter() override;

  // DebugInfoGetter implementation.
  sync_pb::DebugInfo GetDebugInfo() const override;
  void ClearDebugInfo() override;

  void AddDebugEvent();

 private:
  sync_pb::DebugInfo debug_info_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_DEBUG_INFO_GETTER_H_

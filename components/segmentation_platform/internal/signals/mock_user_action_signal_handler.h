// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_MOCK_USER_ACTION_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_MOCK_USER_ACTION_SIGNAL_HANDLER_H_

#include <set>

#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockUserActionSignalHandler : public UserActionSignalHandler {
 public:
  MockUserActionSignalHandler();
  ~MockUserActionSignalHandler() override;

  MOCK_METHOD(void, SetRelevantUserActions, (std::set<uint64_t>));
  MOCK_METHOD(void, EnableMetrics, (bool));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_MOCK_USER_ACTION_SIGNAL_HANDLER_H_

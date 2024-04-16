// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_MOCK_TETHER_HOST_RESPONSE_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_MOCK_TETHER_HOST_RESPONSE_RECORDER_H_

#include <vector>

#include "chromeos/ash/components/tether/tether_host_response_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace tether {

// Test double for TetherHostResponseRecorder.
class MockTetherHostResponseRecorder : public TetherHostResponseRecorder {
 public:
  MockTetherHostResponseRecorder();

  MockTetherHostResponseRecorder(const MockTetherHostResponseRecorder&) =
      delete;
  MockTetherHostResponseRecorder& operator=(
      const MockTetherHostResponseRecorder&) = delete;

  ~MockTetherHostResponseRecorder() override;

  MOCK_METHOD1(RecordSuccessfulTetherAvailabilityResponse,
               void(const std::string&));
  MOCK_METHOD1(RecordSuccessfulConnectTetheringResponse,
               void(const std::string&));
  MOCK_CONST_METHOD0(GetPreviouslyAvailableHostIds, std::vector<std::string>());
  MOCK_CONST_METHOD0(GetPreviouslyConnectedHostIds, std::vector<std::string>());
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_MOCK_TETHER_HOST_RESPONSE_RECORDER_H_

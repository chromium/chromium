// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/mock_tether_host_response_recorder.h"

namespace ash {

namespace tether {

MockTetherHostResponseRecorder::MockTetherHostResponseRecorder()
    : TetherHostResponseRecorder(nullptr) {}

MockTetherHostResponseRecorder::~MockTetherHostResponseRecorder() = default;

}  // namespace tether

}  // namespace ash

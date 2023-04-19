// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/mock_auth_performer.h"

namespace ash {

MockAuthPerformer::MockAuthPerformer(UserDataAuthClient* client)
    : AuthPerformer(client) {}

MockAuthPerformer::~MockAuthPerformer() = default;

}  // namespace ash

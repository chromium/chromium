// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/session.h"

namespace session_manager {

Session::Session(SessionId session_id, const AccountId& account_id)
    : session_id_(session_id), account_id_(account_id) {}

Session::~Session() = default;

}  // namespace session_manager

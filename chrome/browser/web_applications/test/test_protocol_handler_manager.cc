// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_protocol_handler_manager.h"

namespace web_app {

TestProtocolHandlerManager::TestProtocolHandlerManager(Profile* profile)
    : ProtocolHandlerManager(profile) {}

TestProtocolHandlerManager::~TestProtocolHandlerManager() = default;

}  // namespace web_app

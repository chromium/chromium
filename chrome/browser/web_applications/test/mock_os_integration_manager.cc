// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/mock_os_integration_manager.h"

namespace web_app {

MockOsIntegrationManager::MockOsIntegrationManager()
    : OsIntegrationManager(nullptr, nullptr, nullptr, nullptr, nullptr) {}
MockOsIntegrationManager::MockOsIntegrationManager(
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager)
    : OsIntegrationManager(nullptr,
                           nullptr,
                           nullptr,
                           std::move(protocol_handler_manager),
                           nullptr) {}
MockOsIntegrationManager::~MockOsIntegrationManager() = default;

}  // namespace web_app

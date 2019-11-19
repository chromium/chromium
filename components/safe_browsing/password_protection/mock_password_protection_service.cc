// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/mock_password_protection_service.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/db/database_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

MockPasswordProtectionService::MockPasswordProtectionService()
    : PasswordProtectionService(nullptr, nullptr, nullptr) {}

MockPasswordProtectionService::MockPasswordProtectionService(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service)
    : PasswordProtectionService(database_manager,
                                url_loader_factory,
                                history_service) {}

MockPasswordProtectionService::~MockPasswordProtectionService() {}

}  // namespace safe_browsing

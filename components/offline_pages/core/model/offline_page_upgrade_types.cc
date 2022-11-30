// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_upgrade_types.h"

namespace offline_pages {

StartUpgradeResult::StartUpgradeResult()
    : status(StartUpgradeStatus::DB_ERROR) {}

StartUpgradeResult::StartUpgradeResult(StartUpgradeStatus status)
    : status(status) {}

StartUpgradeResult::StartUpgradeResult(StartUpgradeStatus status,
                                       const std::string& digest,
                                       const base::FilePath& file_path)
    : status(status), digest(digest), file_path(file_path) {}

}  // namespace offline_pages

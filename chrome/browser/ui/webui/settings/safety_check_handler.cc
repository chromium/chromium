// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"

SafetyCheckHandler::SafetyCheckHandler() = default;

SafetyCheckHandler::~SafetyCheckHandler() = default;

void SafetyCheckHandler::PerformSafetyCheck() {
  version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  CheckUpdates(version_updater_.get(),
               base::Bind(&SafetyCheckHandler::OnUpdateCheckResult,
                          base::Unretained(this)));
}

void SafetyCheckHandler::CheckUpdates(
    VersionUpdater* version_updater,
    const VersionUpdater::StatusCallback& update_callback) {
  version_updater->CheckForUpdate(update_callback,
                                  VersionUpdater::PromoteCallback());
}

void SafetyCheckHandler::OnUpdateCheckResult(VersionUpdater::Status status,
                                             int progress,
                                             bool rollback,
                                             const std::string& version,
                                             int64_t update_size,
                                             const base::string16& message) {
  NOTIMPLEMENTED();
}

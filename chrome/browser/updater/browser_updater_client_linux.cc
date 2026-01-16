// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace updater {

std::string BrowserUpdaterClient::GetAppId() {
  NOTREACHED() << "Chromium Updater does not manage the browser on Linux";
}

base::FilePath BrowserUpdaterClient::GetExpectedEcp() {
  NOTREACHED() << "Chromium Updater does not manage the browser on Linux";
}

RegistrationRequest BrowserUpdaterClient::GetRegistrationRequest() {
  NOTREACHED() << "Chromium Updater does not manage the browser on Linux";
}

bool BrowserUpdaterClient::AppMatches(const UpdateService::AppState& app) {
  return false;
}

}  // namespace updater

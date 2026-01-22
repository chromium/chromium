// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_RESULT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

// The result of checking if a URL is installable.
enum class InstallableCheckResult {
  kNotInstallable,
  kInstallable,
  kAlreadyInstalled,
};

std::ostream& operator<<(std::ostream& os, InstallableCheckResult result);

using FetchInstallabilityForChromeManagementCallback =
    base::OnceCallback<void(InstallableCheckResult result,
                            std::optional<webapps::AppId> app_id)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_FETCH_INSTALLABILITY_FOR_CHROME_MANAGEMENT_RESULT_H_

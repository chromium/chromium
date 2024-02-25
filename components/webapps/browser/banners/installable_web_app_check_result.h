// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_BANNERS_INSTALLABLE_WEB_APP_CHECK_RESULT_H_
#define COMPONENTS_WEBAPPS_BROWSER_BANNERS_INSTALLABLE_WEB_APP_CHECK_RESULT_H_

namespace webapps {
// Installable describes to what degree a site satisfies the installability
// requirements.
enum class InstallableWebAppCheckResult {
  kUnknown,
  kNo,
  kNo_AlreadyInstalled,
  kYes_ByUserRequest,
  kYes_Promotable,
};
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_BANNERS_INSTALLABLE_WEB_APP_CHECK_RESULT_H_

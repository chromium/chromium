// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/web_app_auto_login_util.h"

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/mac/mac_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"  // nogncheck

namespace web_app {

namespace {
WebAppAutoLoginUtil* g_auto_login_util_for_testing = nullptr;
}

// static
WebAppAutoLoginUtil* WebAppAutoLoginUtil::GetInstance() {
  if (g_auto_login_util_for_testing) {
    return g_auto_login_util_for_testing;
  }

  static base::NoDestructor<WebAppAutoLoginUtil> instance;
  return instance.get();
}

// static
void WebAppAutoLoginUtil::SetInstanceForTesting(
    WebAppAutoLoginUtil* auto_login_util) {
  g_auto_login_util_for_testing = auto_login_util;
}

void WebAppAutoLoginUtil::AddToLoginItems(const base::FilePath& app_bundle_path,
                                          bool hide_on_startup) {
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    os_override->EnableOrDisablePathOnLogin(app_bundle_path,
                                            /*enable_on_login=*/true);
  } else {
    base::mac::AddToLoginItems(app_bundle_path, hide_on_startup);
  }
}

void WebAppAutoLoginUtil::RemoveFromLoginItems(
    const base::FilePath& app_bundle_path) {
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    os_override->EnableOrDisablePathOnLogin(app_bundle_path,
                                            /*enable_on_login=*/false);
  } else {
    base::mac::RemoveFromLoginItems(app_bundle_path);
  }
}

}  // namespace web_app

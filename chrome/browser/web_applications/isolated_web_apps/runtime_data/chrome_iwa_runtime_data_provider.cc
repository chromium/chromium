// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/values.h"

namespace web_app {

namespace {
ChromeIwaRuntimeDataProvider* g_instance = nullptr;
}  // namespace

base::Value
ChromeIwaRuntimeDataProvider::SpecialAppPermissionsInfo::AsDebugValue() const {
  return base::Value(base::DictValue().Set("skip_capture_started_notification",
                                           skip_capture_started_notification));
}

ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData::
    UserInstallAllowlistItemData(const std::string& enterprise_name)
    : enterprise_name(enterprise_name) {}

ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData::
    ~UserInstallAllowlistItemData() = default;

ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData::
    UserInstallAllowlistItemData(const UserInstallAllowlistItemData&) = default;

base::Value
ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData::AsDebugValue()
    const {
  return base::Value(base::DictValue().Set("enterprise_name", enterprise_name));
}

// static
ChromeIwaRuntimeDataProvider& ChromeIwaRuntimeDataProvider::GetInstance() {
  CHECK(g_instance)
      << "ChromeIwaRuntimeDataProvider must be initialized by the time "
         "of the call to GetInstance(). This normally happens in at the "
         "startup of the BrowserProcess (either in BrowserProcessImpl or "
         "TestingBrowserProcess).";
  return *g_instance;
}

// static
void ChromeIwaRuntimeDataProvider::SetInstance(
    base::PassKey<BrowserProcessImpl, TestingBrowserProcess>,
    ChromeIwaRuntimeDataProvider* instance) {
  g_instance = instance;
}

// static
base::AutoReset<ChromeIwaRuntimeDataProvider*>
ChromeIwaRuntimeDataProvider::SetInstanceForTesting(
    ChromeIwaRuntimeDataProvider* instance) {
  CHECK_IS_TEST();
  return base::AutoReset<ChromeIwaRuntimeDataProvider*>(&g_instance, instance);
}

}  // namespace web_app

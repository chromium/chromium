// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_IWA_RUNTIME_DATA_PROVIDER_MIXIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_IWA_RUNTIME_DATA_PROVIDER_MIXIN_H_

#include "chrome/browser/web_applications/isolated_web_apps/test/chrome_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"

namespace web_app {

// A browser test mixin that creates and injects an instance of
// `FakeIwaRuntimeDataProvider`.
//
// Usage:
//
// class MyIwaBrowserTest : public IsolatedWebAppBrowserTestHarness {
//  protected:
//   FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
// };
//
// IN_PROC_BROWSER_TEST_F(MyIwaBrowserTest, MyTest) {
//   data_provider_->Update(
//       [&](auto& update) {
//         update.AddToManagedAllowlist(kTestWebBundleId);
//       });
//   ...
// }
using FakeIwaRuntimeDataProviderMixin =
    TypedIwaRuntimeDataProviderMixin<FakeIwaRuntimeDataProvider>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_IWA_RUNTIME_DATA_PROVIDER_MIXIN_H_

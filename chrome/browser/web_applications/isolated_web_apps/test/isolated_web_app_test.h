// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_TEST_H_

#include <memory>

#include "base/test/task_environment.h"
#include "base/traits_bag.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class IsolatedWebAppTest : public ::testing::Test {
 public:
  struct WithDevMode {};
  struct ValidTraits {
    ValidTraits(base::test::TaskEnvironment::ValidTraits);
    ValidTraits(WithDevMode);
  };

  template <typename... IsolatedWebAppTestTraits>
    requires base::trait_helpers::AreValidTraits<ValidTraits,
                                                 IsolatedWebAppTestTraits...>
  explicit IsolatedWebAppTest(IsolatedWebAppTestTraits&&... traits)
      : IsolatedWebAppTest(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::trait_helpers::Exclude<WithDevMode>::Filter(traits)...),
            /*dev_mode=*/
            base::trait_helpers::HasTrait<WithDevMode,
                                          IsolatedWebAppTestTraits...>()) {}

  ~IsolatedWebAppTest() override;

  TestingProfile* profile();
  FakeWebAppProvider& provider();
  network::TestURLLoaderFactory& url_loader_factory();
  IwaTestServerConfigurator& test_update_server();
  content::BrowserTaskEnvironment& task_environment();

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  IsolatedWebAppTest(std::unique_ptr<content::BrowserTaskEnvironment> env,
                     bool dev_mode);

  base::test::ScopedFeatureList features_;
  std::unique_ptr<content::BrowserTaskEnvironment> env_;

  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;

  // std::unique_ptr so it can be reset in the TearDown method for the safest
  // 'waiting' for os integration to complete, while the task environment is
  // still around.
  std::unique_ptr<web_app::OsIntegrationTestOverrideBlockingRegistration>
      os_integration_test_override_{std::make_unique<
          web_app::OsIntegrationTestOverrideBlockingRegistration>()};

  data_decoder::test::InProcessDataDecoder decoder_;

  network::TestURLLoaderFactory url_loader_factory_;
  IwaTestServerConfigurator test_update_server_{url_loader_factory()};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_TEST_H_

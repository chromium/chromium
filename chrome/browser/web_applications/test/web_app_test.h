// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace web_app {
class FakeWebAppProvider;
}

// Consider to implement web app specific test harness independent of
// RenderViewHost.
// When inheriting from this class, the FakeWebAppProvider doesn't automatically
// start the WebAppProvider system. To do so, try using a helper method like
// `test::AwaitStartWebAppProviderAndSubsystems`.
class WebAppTest : public content::RenderViewHostTestHarness {
 public:
  struct WithTestUrlLoaderFactory {};
  struct ValidTraits {
    explicit ValidTraits(base::test::TaskEnvironment::ValidTraits);
    explicit ValidTraits(WithTestUrlLoaderFactory);
  };

  template <typename... WebAppTestTraits>
    requires base::trait_helpers::AreValidTraits<ValidTraits,
                                                 WebAppTestTraits...>
  explicit WebAppTest(WebAppTestTraits&&... traits)
      : content::RenderViewHostTestHarness(
            base::trait_helpers::Exclude<WithTestUrlLoaderFactory>::Filter(
                traits)...) {
    shared_url_loader_factory_ =
        base::trait_helpers::HasTrait<WithTestUrlLoaderFactory,
                                      WebAppTestTraits...>()
            ? test_url_loader_factory_.GetSafeWeakWrapper()
            : nullptr;
  }

  ~WebAppTest() override;

  void SetUp() override;
  void TearDown() override;

  TestingProfile* profile() const { return profile_.get(); }
  TestingProfileManager& profile_manager() { return testing_profile_manager_; }

  network::TestURLLoaderFactory& profile_url_loader_factory() {
    CHECK(shared_url_loader_factory_)
        << "If you want the testing profile to use a "
           "`network::TestURLLoaderFactory`, make sure to pass the "
           "`WebAppTest::WithTestUrlLoaderFactory` trait to the constructor of "
           "`WebAppTest`. By default, the testing profile's `URLLoaderFactory` "
           "will be `nullptr`.";
    return test_url_loader_factory_;
  }

  web_app::FakeWebAppProvider& fake_provider() const;

  web_app::OsIntegrationTestOverrideImpl& fake_os_integration() const;

 protected:
  // content::RenderViewHostTestHarness.
  content::BrowserContext* GetBrowserContext() final;

 private:
  base::TimeTicks start_time_ = base::TimeTicks::Now();
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_ =
      nullptr;

  // std::unique_ptr so it can be reset in the TearDown method for the safest
  // 'waiting' for os integration to complete, while the task environment is
  // still around.
  std::unique_ptr<web_app::OsIntegrationTestOverrideBlockingRegistration>
      os_integration_test_override_{std::make_unique<
          web_app::OsIntegrationTestOverrideBlockingRegistration>()};

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

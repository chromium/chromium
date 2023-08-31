// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
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
class WebAppTest : public content::RenderViewHostTestHarness {
 public:
  struct WithTestUrlLoaderFactory {};
  struct ValidTraits {
    explicit ValidTraits(base::test::TaskEnvironment::ValidTraits);
    explicit ValidTraits(WithTestUrlLoaderFactory);
  };

  template <typename... WebAppTestTraits,
            class CheckArgumentsAreValid = std::enable_if_t<
                base::trait_helpers::
                    AreValidTraits<ValidTraits, WebAppTestTraits...>::value>>
  explicit WebAppTest(WebAppTestTraits&&... traits)
      : content::RenderViewHostTestHarness(
            base::trait_helpers::Exclude<WithTestUrlLoaderFactory>::Filter(
                traits)...),
        shared_url_loader_factory_(
            base::trait_helpers::HasTrait<WithTestUrlLoaderFactory,
                                          WebAppTestTraits...>()
                ? base::MakeRefCounted<
                      network::WeakWrapperSharedURLLoaderFactory>(
                      &test_url_loader_factory_)
                : nullptr) {}

  ~WebAppTest() override;

  void SetUp() override;
  void TearDown() override;

  TestingProfile* profile() { return profile_.get(); }
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

  web_app::FakeWebAppProvider& fake_provider();

 protected:
  // content::RenderViewHostTestHarness.
  content::BrowserContext* GetBrowserContext() final;

 private:
  base::TimeTicks start_time_ = base::TimeTicks::Now();
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_ =
      nullptr;
  network::TestURLLoaderFactory test_url_loader_factory_;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

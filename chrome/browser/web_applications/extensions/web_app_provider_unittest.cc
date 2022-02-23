// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/testing_profile.h"

namespace web_app {

class WebAppProviderUnitTest : public WebAppTest {
 public:
  WebAppProviderUnitTest() = default;
  WebAppProviderUnitTest(const WebAppProviderUnitTest&) = delete;
  WebAppProviderUnitTest& operator=(const WebAppProviderUnitTest&) = delete;
  ~WebAppProviderUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    SkipMainProfileCheckForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    provider_ = WebAppProvider::GetForLocalAppsUnchecked(profile());
  }

  WebAppProvider* provider() { return provider_; }

 private:
  raw_ptr<WebAppProvider> provider_;
};

TEST_F(WebAppProviderUnitTest, Registrar) {
  WebAppRegistrar& registrar = provider()->registrar();
  EXPECT_FALSE(registrar.IsInstalled("unknown"));
}

}  // namespace web_app

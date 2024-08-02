// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_annotations/user_annotations_features.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#endif

namespace user_annotations {

class UserAnnotationsServiceDisabledBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(kUserAnnotations);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceDisabledBrowserTest,
                       ServiceNotCreatedWhenFeatureDisabled) {
  ASSERT_FALSE(
      UserAnnotationsServiceFactory::GetForProfile(browser()->profile()));
}

class UserAnnotationsServiceKioskModeBrowserTest : public InProcessBrowserTest {
 public:
  UserAnnotationsServiceKioskModeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kUserAnnotations);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kKioskMode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceKioskModeBrowserTest,
                       DisabledInKioskMode) {
  EXPECT_EQ(nullptr,
            UserAnnotationsServiceFactory::GetForProfile(browser()->profile()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class UserAnnotationsServiceEphemeralProfileBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  UserAnnotationsServiceEphemeralProfileBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kUserAnnotations);
  }

 private:
  ash::GuestSessionMixin guest_session_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceEphemeralProfileBrowserTest,
                       EphemeralProfileDoesNotInstantiateService) {
  EXPECT_EQ(nullptr,
            UserAnnotationsServiceFactory::GetForProfile(browser()->profile()));
}
#endif

class UserAnnotationsServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

 protected:
  UserAnnotationsService* service() {
    return UserAnnotationsServiceFactory::GetForProfile(browser()->profile());
  }

  virtual void InitializeFeatureList() {
    feature_list_.InitAndEnableFeature(kUserAnnotations);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest, ServiceFactoryWorks) {
  EXPECT_TRUE(service());
}

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest,
                       ServiceNotCreatedForIncognito) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_EQ(nullptr, UserAnnotationsServiceFactory::GetForProfile(
                         otr_browser->profile()));
}

}  // namespace user_annotations

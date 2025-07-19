// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_fieldtrial.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"

class NtpComposeboxFieldTrialBrowserTest : public InProcessBrowserTest {
 public:
  NtpComposeboxFieldTrialBrowserTest() {
    feature_list_.InitWithFeatures(
        {ntp_composebox::kNtpSearchboxComposeEntrypointEnglishUS},
        {ntp_composebox::kNtpSearchboxComposeEntrypoint});
  }
  ~NtpComposeboxFieldTrialBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class NtpComposeboxFieldTrialDisableEntrypointBrowserTest
    : public InProcessBrowserTest {
 public:
  NtpComposeboxFieldTrialDisableEntrypointBrowserTest() {
    feature_list_.InitWithFeatures(
        {}, {ntp_composebox::kNtpSearchboxComposeEntrypointEnglishUS,
             ntp_composebox::kNtpSearchboxComposeEntrypoint});
  }
  ~NtpComposeboxFieldTrialDisableEntrypointBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class NtpComposeboxFieldTrialOverrideEntrypointBrowserTest
    : public InProcessBrowserTest {
 public:
  NtpComposeboxFieldTrialOverrideEntrypointBrowserTest() {
    feature_list_.InitWithFeatures(
        {ntp_composebox::kNtpSearchboxComposeEntrypoint},
        {ntp_composebox::kNtpSearchboxComposeEntrypointEnglishUS});
  }
  ~NtpComposeboxFieldTrialOverrideEntrypointBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpComposeboxFieldTrialBrowserTest,
                       EnableEntrypointUSEnglishTot) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  // Once enabled by default, change to EXPECT_TRUE.
  EXPECT_FALSE(ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(
      g_browser_process));
}

IN_PROC_BROWSER_TEST_F(NtpComposeboxFieldTrialBrowserTest,
                       DisableEntrypointForNonUSEnglish) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-CA");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(
      g_browser_process));
}

IN_PROC_BROWSER_TEST_F(NtpComposeboxFieldTrialDisableEntrypointBrowserTest,
                       DisableEntrypointUSEnglishTot) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(
      g_browser_process));
}

IN_PROC_BROWSER_TEST_F(NtpComposeboxFieldTrialDisableEntrypointBrowserTest,
                       DisableEntrypointForNonUSEnglish) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-CA");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(
      g_browser_process));
}

IN_PROC_BROWSER_TEST_F(NtpComposeboxFieldTrialOverrideEntrypointBrowserTest,
                       OverrideEntrypointFeature) {
  // If `kNtpSearchboxComposeEntrypoint` is overridden, show entrypoint even if
  // `kNtpSearchboxComposeEntrypointEnglishUS` is disabled.
  EXPECT_TRUE(ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(
      g_browser_process));
}

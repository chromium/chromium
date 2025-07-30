// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <tuple>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"

class NtpComposeboxFieldTrialEntrypointBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<std::string,
                                                      std::string,
                                                      std::optional<bool>,
                                                      std::optional<bool>>> {
 public:
  NtpComposeboxFieldTrialEntrypointBrowserTest() = default;
  ~NtpComposeboxFieldTrialEntrypointBrowserTest() override = default;

 protected:
  void SetUp() override {
    auto entrypoint = std::get<2>(GetParam());
    auto entrypoint_en_us = std::get<3>(GetParam());
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (entrypoint.has_value()) {
      if (*entrypoint) {
        enabled_features.push_back(
            ntp_composebox::kNtpSearchboxComposeEntrypoint);
      } else {
        disabled_features.push_back(
            ntp_composebox::kNtpSearchboxComposeEntrypoint);
      }
    }
    if (entrypoint_en_us.has_value()) {
      if (*entrypoint_en_us) {
        enabled_features.push_back(
            ntp_composebox::kNtpSearchboxComposeEntrypointEnglishUS);
      } else {
        disabled_features.push_back(
            ntp_composebox::kNtpSearchboxComposeEntrypointEnglishUS);
      }
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    scoped_browser_locale_ =
        std::make_unique<ScopedBrowserLocale>(std::get<0>(GetParam()));
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        std::get<1>(GetParam()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_locale_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedBrowserLocale> scoped_browser_locale_;
};

INSTANTIATE_TEST_SUITE_P(,
                         NtpComposeboxFieldTrialEntrypointBrowserTest,
                         ::testing::Combine(
                             // Values for the locale.
                             ::testing::Values("en-US", "es-MX"),
                             // Values for the country.
                             ::testing::Values("us", "ca"),
                             // Values for the generic entrypoint feature state.
                             ::testing::Values(std::nullopt, true, false),
                             // Values for the en-US entrypoint feature state.
                             ::testing::Values(std::nullopt, true, false)));

IN_PROC_BROWSER_TEST_P(NtpComposeboxFieldTrialEntrypointBrowserTest, Test) {
  auto [locale, country, entrypoint, entrypoint_english_us] = GetParam();
  bool expected_enabled;
  if (locale == "en-US" && country == "us") {
    // For en-US in US, the generic `entrypoint` feature takes precedence. If
    // not set, the `entrypoint_english_us` feature is checked.
    if (entrypoint.has_value()) {
      expected_enabled = *entrypoint;
    } else if (entrypoint_english_us.has_value()) {
      expected_enabled = *entrypoint_english_us;
    } else {
      // Fallback to the default state of the en-US specific feature.
      expected_enabled = base::FeatureList::IsEnabled(
          ntp_composebox::kNtpSearchboxComposeEntrypointEnglishUS);
    }
  } else {
    // For all other countries and locales, only the generic `entrypoint`
    // feature matters. The `entrypoint_english_us` feature is ignored.
    if (entrypoint.has_value()) {
      expected_enabled = *entrypoint;
    } else {
      // Fallback to the default state of the generic feature.
      expected_enabled = base::FeatureList::IsEnabled(
          ntp_composebox::kNtpSearchboxComposeEntrypoint);
    }
  }
  EXPECT_EQ(
      ntp_composebox::IsNtpSearchboxComposeEntrypointEnabled(g_browser_process),
      expected_enabled);
}

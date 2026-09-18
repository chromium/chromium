// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/utils.h"

#include <optional>
#include <string>

#include "components/lens/lens_overlay_invocation_source.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "url/gurl.h"

namespace contextual_tasks {
namespace {

TEST(ContextualTasksUtilsTest, GetLensInvocationSourceForAimZeroState) {
  EXPECT_EQ(
      GetLensInvocationSourceForAimZeroState(
          omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON),
      lens::LensOverlayInvocationSource::kCobrowseToolbarButton);
  EXPECT_EQ(GetLensInvocationSourceForAimZeroState(
                omnibox::ChromeAimEntryPoint::
                    DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON),
            lens::LensOverlayInvocationSource::kCobrowsePinnedToolbarButton);
  EXPECT_EQ(
      GetLensInvocationSourceForAimZeroState(
          omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION),
      lens::LensOverlayInvocationSource::kOmniboxPageAction);
  EXPECT_EQ(GetLensInvocationSourceForAimZeroState(
                omnibox::ChromeAimEntryPoint::
                    DESKTOP_CHROME_COBROWSE_OMNIBOX_TAB_SEARCH),
            lens::LensOverlayInvocationSource::kOmniboxPageAction);
  EXPECT_EQ(GetLensInvocationSourceForAimZeroState(
                omnibox::ChromeAimEntryPoint::IOS_CHROME_APP_BAR_ENTRY_POINT),
            lens::LensOverlayInvocationSource::kAppBarAimButton);
  EXPECT_EQ(GetLensInvocationSourceForAimZeroState(
                omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT),
            std::nullopt);
}

TEST(ContextualTasksUtilsTest,
     AppendAimEntryPointParams_CobrowseOmniboxAction) {
  GURL base_url("https://www.google.com");
  GURL result_url = AppendAimEntryPointParams(
      base_url,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);

  std::string aep;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "aep", &aep));
  EXPECT_EQ(aep, "205");

  std::string source;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "source", &source));
  EXPECT_EQ(source, "chrome.crn.obpa");
}

TEST(ContextualTasksUtilsTest,
     AppendAimEntryPointParams_CobrowseOmniboxTabSearch) {
  GURL base_url("https://www.google.com");
  GURL result_url = AppendAimEntryPointParams(
      base_url,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_TAB_SEARCH);

  std::string aep;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "aep", &aep));
  EXPECT_EQ(aep, "206");

  std::string source;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "source", &source));
  EXPECT_EQ(source, "chrome.crn.obpa");
}

TEST(ContextualTasksUtilsTest,
     AppendAimEntryPointParams_CobrowseToolbarButton) {
  GURL base_url("https://www.google.com");
  GURL result_url = AppendAimEntryPointParams(
      base_url,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);

  std::string aep;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "aep", &aep));
  EXPECT_EQ(aep, "130");

  std::string source;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "source", &source));
  EXPECT_EQ(source, "chrome.crn.cct");
}

TEST(ContextualTasksUtilsTest,
     AppendAimEntryPointParams_CobrowsePinnedToolbarButton) {
  GURL base_url("https://www.google.com");
  GURL result_url = AppendAimEntryPointParams(
      base_url, omnibox::ChromeAimEntryPoint::
                    DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON);

  std::string aep;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "aep", &aep));
  EXPECT_EQ(aep, "175");

  std::string source;
  EXPECT_TRUE(net::GetValueForKeyInQuery(result_url, "source", &source));
  EXPECT_EQ(source, "chrome.crn.ccpt");
}

TEST(ContextualTasksUtilsTest, AppendAimEntryPointParams_UnknownEntryPoint) {
  GURL base_url("https://www.google.com");
  GURL result_url = AppendAimEntryPointParams(
      base_url, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);

  EXPECT_EQ(result_url, base_url);
}

TEST(ContextualTasksUtilsTest, GetDarkModeFromUrl) {
  EXPECT_EQ(GetDarkModeFromUrl(GURL("https://www.google.com?cs=0")), false);
  EXPECT_EQ(GetDarkModeFromUrl(GURL("https://www.google.com?cs=1")), true);
  EXPECT_EQ(GetDarkModeFromUrl(GURL("https://www.google.com?cs=2")),
            std::nullopt);
  EXPECT_EQ(GetDarkModeFromUrl(GURL("https://www.google.com")), std::nullopt);
}

}  // namespace
}  // namespace contextual_tasks

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/local_ntp_browsertest_base.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"

namespace {

// Delimiter in the Most Visited icon URL that indicates a dark icon. Keep value
// in sync with NtpIconSource.
const char kMVIconDarkParameter[] = "dark=true";

}  // namespace

TestInstantServiceObserver::TestInstantServiceObserver(InstantService* service)
    : service_(service) {
  service_->AddObserver(this);
}

TestInstantServiceObserver::~TestInstantServiceObserver() {
  service_->RemoveObserver(this);
}

void TestInstantServiceObserver::WaitForMostVisitedItems(size_t count) {
  DCHECK(!quit_closure_most_visited_);

  expected_count_ = count;

  if (items_.size() == count) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_most_visited_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestInstantServiceObserver::WaitForNtpThemeUpdated(
    std::string background_url,
    std::string attribution_1,
    std::string attribution_2,
    std::string attribution_action_url) {
  DCHECK(!quit_closure_custom_background_);

  expected_background_url_ = background_url;
  expected_attribution_1_ = attribution_1;
  expected_attribution_2_ = attribution_2;
  expected_attribution_action_url_ = attribution_action_url;

  if (theme_.custom_background_url == background_url &&
      theme_.custom_background_attribution_line_1 == attribution_1 &&
      theme_.custom_background_attribution_line_2 == attribution_2 &&
      theme_.custom_background_attribution_action_url ==
          attribution_action_url) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_custom_background_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestInstantServiceObserver::WaitForThemeApplied(bool theme_installed) {
  DCHECK(!quit_closure_theme_);

  theme_installed_ = theme_installed;
  if (!theme_.using_default_theme == theme_installed) {
    return;
  }

  base::RunLoop run_loop;
  quit_closure_theme_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool TestInstantServiceObserver::IsUsingDefaultTheme() {
  return theme_.using_default_theme;
}

void TestInstantServiceObserver::NtpThemeChanged(const NtpTheme& theme) {
  theme_ = theme;

  if (quit_closure_custom_background_ &&
      theme_.custom_background_url == expected_background_url_ &&
      theme_.custom_background_attribution_line_1 == expected_attribution_1_ &&
      theme_.custom_background_attribution_line_2 == expected_attribution_2_ &&
      theme_.custom_background_attribution_action_url ==
          expected_attribution_action_url_) {
    // Exit when the custom background was applied successfully.
    std::move(quit_closure_custom_background_).Run();
    quit_closure_custom_background_.Reset();
  } else if (quit_closure_theme_ &&
             !theme_.using_default_theme == theme_installed_) {
    // Exit when the theme was applied successfully.
    std::move(quit_closure_theme_).Run();
    quit_closure_theme_.Reset();
  }
}

void TestInstantServiceObserver::MostVisitedInfoChanged(
    const InstantMostVisitedInfo& most_visited_info) {
  items_ = most_visited_info.items;

  if (quit_closure_most_visited_ && items_.size() == expected_count_) {
    std::move(quit_closure_most_visited_).Run();
    quit_closure_most_visited_.Reset();
  }
}

DarkModeTestBase::DarkModeTestBase() {}

bool DarkModeTestBase::GetIsDarkModeApplied(
    const content::ToRenderFrameHost& frame) {
  bool dark_mode_applied = false;
  if (instant_test_utils::GetBoolFromJS(frame,
                                        " window.matchMedia('(prefers-color-"
                                        "scheme: dark)').matches === true",
                                        &dark_mode_applied)) {
    return dark_mode_applied;
  }
  return false;
}

bool DarkModeTestBase::GetIsLightChipsApplied(
    const content::ToRenderFrameHost& frame) {
  bool light_chips_applied = false;
  if (instant_test_utils::GetBoolFromJS(
          frame, "document.body.classList.contains('light-chip')",
          &light_chips_applied)) {
    return light_chips_applied;
  }
  return false;
}

bool DarkModeTestBase::GetIsDarkTile(const content::ToRenderFrameHost& frame,
                                     int index) {
  bool dark_tile = false;
  if (instant_test_utils::GetBoolFromJS(
          frame,
          base::StringPrintf(
              "document.querySelectorAll('#mv-tiles .md-icon img')[%d]"
              ".src.includes('%s')",
              index, kMVIconDarkParameter),
          &dark_tile)) {
    return dark_tile;
  }
  return false;
}

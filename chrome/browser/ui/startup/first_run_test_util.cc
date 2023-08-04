// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_test_util.h"

#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

void SetIsFirstRun(bool is_first_run) {
  // We want this to be functional when called from the test body because
  // enabling the FRE to run in the pre-test setup would prevent opening the
  // browser that the test fixtures rely on.
  // So are manipulating flags here instead of during `SetUpX` methods on
  // purpose.
  if (first_run::IsChromeFirstRun() == is_first_run) {
    return;
  }

  if (is_first_run) {
    // This switch is added by InProcessBrowserTest
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  } else {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }

  first_run::ResetCachedSentinelDataForTesting();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_EQ(is_first_run, first_run::IsChromeFirstRun());
  }
}

bool GetFirstRunFinishedPrefValue() {
  CHECK(g_browser_process->local_state());
  return g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished);
}

// -- FirstRunServiceBrowserTestBase -------------------------------------------

FirstRunServiceBrowserTestBase::FirstRunServiceBrowserTestBase() = default;

FirstRunServiceBrowserTestBase::~FirstRunServiceBrowserTestBase() = default;

void FirstRunServiceBrowserTestBase::SetUpOnMainThread() {
  // We can remove flags and state suppressing the first run only after the
  // browsertest's initial browser is opened. Otherwise we would have to
  // close the FRE and reset its state before each individual test.
  SetIsFirstRun(true);

  // Also make sure we will do another attempt at creating the service now
  // that the first run state changed.
  ASSERT_FALSE(FirstRunServiceFactory::GetForBrowserContextIfExists(profile()));
  FirstRunServiceFactory::GetInstance()->Disassociate(profile());
}

Profile* FirstRunServiceBrowserTestBase::profile() const {
  return browser()->profile();
}

FirstRunService* FirstRunServiceBrowserTestBase::fre_service() const {
  return FirstRunServiceFactory::GetForBrowserContext(profile());
}

std::u16string FirstRunServiceBrowserTestBase::GetProfileName() const {
  return g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile()->GetPath())
      ->GetLocalProfileName();
}

bool FirstRunServiceBrowserTestBase::IsUsingDefaultProfileName() const {
  return g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile()->GetPath())
      ->IsUsingDefaultName();
}

bool FirstRunServiceBrowserTestBase::IsProfileNameDefault() const {
  auto& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  bool is_default = storage.IsDefaultProfileName(
      GetProfileName(),
      /*include_check_for_legacy_profile_name=*/false);

  EXPECT_EQ(storage.GetProfileAttributesWithPath(profile()->GetPath())
                ->IsUsingDefaultName(),
            is_default);

  return is_default;
}

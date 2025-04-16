// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

#include <utility>

#include "base/containers/adapters.h"

InProcessBrowserTestMixin::InProcessBrowserTestMixin(
    InProcessBrowserTestMixinHost* host) {
  host->mixins_.push_back(this);
}

void InProcessBrowserTestMixin::SetUp() {}

void InProcessBrowserTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {}

void InProcessBrowserTestMixin::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {}

bool InProcessBrowserTestMixin::SetUpUserDataDirectory() {
  return true;
}

void InProcessBrowserTestMixin::SetUpInProcessBrowserTestFixture() {}

void InProcessBrowserTestMixin::SetUpLocalStatePrefService(
    PrefService* local_state) {}

void InProcessBrowserTestMixin::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {}

void InProcessBrowserTestMixin::SetUpOnMainThread() {}

void InProcessBrowserTestMixin::TearDownOnMainThread() {}

void InProcessBrowserTestMixin::PostRunTestOnMainThread() {}

void InProcessBrowserTestMixin::TearDownInProcessBrowserTestFixture() {}

void InProcessBrowserTestMixin::TearDown() {}

InProcessBrowserTestMixinHost::InProcessBrowserTestMixinHost() = default;

InProcessBrowserTestMixinHost::~InProcessBrowserTestMixinHost() = default;

void InProcessBrowserTestMixinHost::SetUp() {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->SetUp();
  }
}

void InProcessBrowserTestMixinHost::SetUpCommandLine(
    base::CommandLine* command_line) {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->SetUpCommandLine(command_line);
  }
}

void InProcessBrowserTestMixinHost::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->SetUpDefaultCommandLine(command_line);
  }
}

bool InProcessBrowserTestMixinHost::SetUpUserDataDirectory() {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    if (!mixin->SetUpUserDataDirectory()) {
      return false;
    }
  }
  return true;
}

void InProcessBrowserTestMixinHost::SetUpInProcessBrowserTestFixture() {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->SetUpInProcessBrowserTestFixture();
  }
}

void InProcessBrowserTestMixinHost::SetUpLocalStatePrefService(
    PrefService* local_state) {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->SetUpLocalStatePrefService(local_state);
  }
}

void InProcessBrowserTestMixinHost::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->CreatedBrowserMainParts(browser_main_parts);
  }
}

void InProcessBrowserTestMixinHost::SetUpOnMainThread() {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->SetUpOnMainThread();
  }
}

void InProcessBrowserTestMixinHost::TearDownOnMainThread() {
  for (InProcessBrowserTestMixin* mixin : base::Reversed(mixins_)) {
    mixin->TearDownOnMainThread();
  }
}

void InProcessBrowserTestMixinHost::PostRunTestOnMainThread() {
  for (InProcessBrowserTestMixin* mixin : mixins_) {
    mixin->PostRunTestOnMainThread();
  }
}

void InProcessBrowserTestMixinHost::TearDownInProcessBrowserTestFixture() {
  for (InProcessBrowserTestMixin* mixin : base::Reversed(mixins_)) {
    mixin->TearDownInProcessBrowserTestFixture();
  }
}

void InProcessBrowserTestMixinHost::TearDown() {
  for (InProcessBrowserTestMixin* mixin : base::Reversed(mixins_)) {
    mixin->TearDown();
  }
}

// Implementation of MixinBasedInProcessBrowserTest.
#if BUILDFLAG(IS_ANDROID)
template class InProcessBrowserTestMixinHostSupport<AndroidBrowserTest>;
#else
template class InProcessBrowserTestMixinHostSupport<InProcessBrowserTest>;
#endif

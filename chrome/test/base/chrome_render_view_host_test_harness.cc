// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#endif

using content::RenderViewHostTester;
using content::RenderViewHostTestHarness;

ChromeRenderViewHostTestHarness::~ChromeRenderViewHostTestHarness() = default;

TestingProfile* ChromeRenderViewHostTestHarness::profile() {
  return static_cast<TestingProfile*>(browser_context());
}

void ChromeRenderViewHostTestHarness::TearDown() {
  RenderViewHostTestHarness::TearDown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::Shell::DeleteInstance();
#endif
}

TestingProfile::TestingFactories
ChromeRenderViewHostTestHarness::GetTestingFactories() const {
  return {};
}

std::unique_ptr<TestingProfile>
ChromeRenderViewHostTestHarness::CreateTestingProfile(
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    bool is_main_profile
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
) {
  TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  builder.SetIsMainProfile(is_main_profile);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  builder.AddTestingFactories(GetTestingFactories());

  return builder.Build();
}

std::unique_ptr<content::BrowserContext>
ChromeRenderViewHostTestHarness::CreateBrowserContext() {
  return CreateTestingProfile(
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      /*is_main_profile=*/true
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );
}

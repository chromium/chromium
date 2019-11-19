// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"

#if defined(OS_CHROMEOS)
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
#if defined(OS_CHROMEOS)
  ash::Shell::DeleteInstance();
#endif
}

TestingProfile::TestingFactories
ChromeRenderViewHostTestHarness::GetTestingFactories() const {
  return {};
}

std::unique_ptr<TestingProfile>
ChromeRenderViewHostTestHarness::CreateTestingProfile() {
  // Maintain the profile directory ourselves so that it isn't deleted along
  // with TestingProfile.  RenderViewHostTestHarness::TearDown() will destroy
  // the profile and also destroy the thread bundle to ensure that any tasks
  // posted throughout the test run to completion.  By postponing the deletion
  // of the profile directory until ~ChromeRenderViewHostTestHarness() we
  // guarantee that no tasks will try to access the profile directory when it's
  // (being) deleted.
  auto temp_dir = std::make_unique<base::ScopedTempDir>();
  CHECK(temp_dir->CreateUniqueTempDir());

  TestingProfile::Builder builder;
  builder.SetPath(temp_dir->GetPath());

  for (auto& pair : GetTestingFactories())
    builder.AddTestingFactory(pair.first, pair.second);

  temp_dirs_.push_back(std::move(temp_dir));

  return builder.Build();
}

std::unique_ptr<content::BrowserContext>
ChromeRenderViewHostTestHarness::CreateBrowserContext() {
  return CreateTestingProfile();
}

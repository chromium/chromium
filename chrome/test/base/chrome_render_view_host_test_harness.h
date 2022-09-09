// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_

#include <memory>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_renderer_host.h"

// Wrapper around RenderViewHostTestHarness that uses a TestingProfile as
// browser context instead of a TestBrowserContext.
class ChromeRenderViewHostTestHarness
    : public content::RenderViewHostTestHarness {
 public:
  // Construct a ChromeRenderViewHostTestHarness with zero or more arguments
  // passed to content::RenderViewHostTestHarness.
  template <typename... TaskEnvironmentTraits>
  explicit ChromeRenderViewHostTestHarness(TaskEnvironmentTraits&&... traits)
      : content::RenderViewHostTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...) {}

  ~ChromeRenderViewHostTestHarness() override;

  TestingProfile* profile();

 protected:
  // testing::Test
  void TearDown() override;

  // Returns a list of factories to use when creating the TestingProfile.
  // Can be overridden by sub-classes if needed.
  virtual TestingProfile::TestingFactories GetTestingFactories() const;

  // Creates a TestingProfile to use as the browser context.
  std::unique_ptr<TestingProfile> CreateTestingProfile(
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      bool is_main_profile = false
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );

  // content::RenderViewHostTestHarness.
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() final;
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_

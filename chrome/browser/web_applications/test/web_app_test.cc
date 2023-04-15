// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test.h"

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

WebAppTest::WebAppTest() = default;

void WebAppTest::SetUp() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  profile_ = testing_profile_manager_.CreateTestingProfile(
      TestingProfile::kDefaultProfileUserName, /*is_main_profile=*/true);
  content::RenderViewHostTestHarness::SetUp();
}

void WebAppTest::TearDown() {
  // RenderViewHostTestHarness::TearDown destroys the TaskEnvironment. We need
  // to destroy profiles before that happens, and web contents need to be
  // destroyed before profiles are destroyed.
  DeleteContents();
  // Make sure that we flush any messages related to WebContentsImpl destruction
  // before we destroy the profiles.
  base::RunLoop().RunUntilIdle();
  testing_profile_manager_.DeleteAllTestingProfiles();
  content::RenderViewHostTestHarness::TearDown();
}

content::BrowserContext* WebAppTest::GetBrowserContext() {
  return profile();
}

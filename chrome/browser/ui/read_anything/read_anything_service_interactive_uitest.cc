// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/read_anything/read_anything_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

class ReadAnythingServiceDataCollectionCUJTest : public InteractiveBrowserTest {
 public:
  ReadAnythingServiceDataCollectionCUJTest() = default;
  ~ReadAnythingServiceDataCollectionCUJTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDataCollectionModeForScreen2x);
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingServiceDataCollectionCUJTest,
                       SidePanelOpensAutomatically) {
  RunTestSequence(WaitForShow(kSidePanelElementId));
}

#if !BUILDFLAG(IS_CHROMEOS)

using ReadAnythingServiceGuestTest = InProcessBrowserTest;
IN_PROC_BROWSER_TEST_F(ReadAnythingServiceGuestTest,
                       ServiceIsCreatedForGuestProfile) {
  Browser* guest_browser = CreateGuestBrowser();
  Profile* guest_profile = guest_browser->profile();
  EXPECT_TRUE(guest_profile->IsGuestSession());

  ReadAnythingService* guest_service = ReadAnythingService::Get(guest_profile);
  EXPECT_NE(nullptr, guest_service);

  // The service should not be created for the original profile because the
  // guest profile uses kOffTheRecord.
  Profile* original_profile = guest_profile->GetOriginalProfile();
  ReadAnythingService* original_service =
      ReadAnythingService::Get(original_profile);
  EXPECT_EQ(nullptr, original_service);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

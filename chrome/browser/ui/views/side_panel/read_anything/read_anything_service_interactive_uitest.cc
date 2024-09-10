// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_service.h"
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

}  // namespace

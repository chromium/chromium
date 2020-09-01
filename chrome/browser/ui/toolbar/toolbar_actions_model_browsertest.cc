// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"

class ToolbarActionsModelBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  ToolbarActionsModelBrowserTest() = default;
  ~ToolbarActionsModelBrowserTest() override = default;

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ToolbarActionsModelBrowserTest, PRE_PinnedStateMetrics) {
  const extensions::Extension* extension1 =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension1);

  const extensions::Extension* extension2 =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));
  ASSERT_TRUE(extension2);

  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile());
  EXPECT_EQ(2u, model->action_ids().size());

  EXPECT_FALSE(model->IsActionPinned(extension1->id()));
  EXPECT_FALSE(model->IsActionPinned(extension2->id()));
  model->SetActionVisibility(extension1->id(), true);
  EXPECT_TRUE(model->IsActionPinned(extension1->id()));
  EXPECT_FALSE(model->IsActionPinned(extension2->id()));
}

IN_PROC_BROWSER_TEST_F(ToolbarActionsModelBrowserTest, PinnedStateMetrics) {
  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile());
  EXPECT_EQ(2u, model->action_ids().size());
  EXPECT_EQ(1u, model->pinned_action_ids().size());

  histogram_tester()->ExpectUniqueSample(
      "Extensions.Toolbar.PinnedExtensionPercentage3", 50, 1);
  histogram_tester()->ExpectUniqueSample(
      "Extensions.Toolbar.PinnedExtensionCount2", 1, 1);
}

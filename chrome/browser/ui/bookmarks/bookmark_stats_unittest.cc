// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using toolbar_ui_api::mojom::PageActionTrigger;

TEST(BookmarkStatsTest, RecordBookmarkPageActionExecution) {
  base::HistogramTester histogram_tester;

  RecordBookmarkPageActionExecution(PageActionTrigger::kMouse);
  histogram_tester.ExpectUniqueSample("Bookmarks.EntryPoint",
                                      BookmarkEntryPoint::kStarMouse, 1);

  RecordBookmarkPageActionExecution(PageActionTrigger::kKeyboard);
  histogram_tester.ExpectBucketCount("Bookmarks.EntryPoint",
                                     BookmarkEntryPoint::kStarKey, 1);

  RecordBookmarkPageActionExecution(PageActionTrigger::kGesture);
  histogram_tester.ExpectBucketCount("Bookmarks.EntryPoint",
                                     BookmarkEntryPoint::kStarGesture, 1);
}

}  // namespace

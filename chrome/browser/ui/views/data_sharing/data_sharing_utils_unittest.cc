// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"

#include "base/test/bind.h"
#include "components/data_sharing/public/group_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTitle[] = "foo_title";
const char kUrl[] = "https://www.foo.com?foo=1";

data_sharing::SharedDataPreview MockPreviewWithTitle(std::string title) {
  data_sharing::SharedTabGroupPreview tab_group_preview;
  tab_group_preview.title = title;
  tab_group_preview.tabs.emplace_back(GURL(kUrl));
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = tab_group_preview;

  return preview;
}
}  // namespace

TEST(DataSharingUtils, ProcessPreviewWithValidTitle) {
  data_sharing::mojom::PageHandler::GetTabGroupPreviewCallback callback =
      base::BindLambdaForTesting(
          [&](data_sharing::mojom::GroupPreviewPtr preview) {
            EXPECT_EQ(preview->title, kTitle);
            EXPECT_EQ(preview->shared_tabs.size(), size_t(1));
            // Trimmed from `kUrl`.
            EXPECT_EQ(preview->shared_tabs[0]->display_url, "foo.com");
            EXPECT_EQ(preview->shared_tabs[0]->favicon_url,
                      GURL("chrome://favicon2"));
            EXPECT_EQ(preview->status_code,
                      mojo_base::mojom::AbslStatusCode::kOk);
          });
  data_sharing::ProcessPreviewOutcome(std::move(callback),
                                      MockPreviewWithTitle(kTitle));
}

TEST(DataSharingUtils, ProcessPreviewWithEmptyTitle) {
  data_sharing::mojom::PageHandler::GetTabGroupPreviewCallback callback =
      base::BindLambdaForTesting(
          [&](data_sharing::mojom::GroupPreviewPtr preview) {
  // The unnamed group has 1 tab.
#if BUILDFLAG(IS_MAC)
            EXPECT_EQ(preview->title, "1 Tab");
#else
            EXPECT_EQ(preview->title, "1 tab");
#endif  // BUILDFLAG(IS_MAC)
          });
  data_sharing::ProcessPreviewOutcome(std::move(callback),
                                      MockPreviewWithTitle(std::string()));
}

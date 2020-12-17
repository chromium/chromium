// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/share_target_utils.h"

#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(ShareTargetUtils, ExtractTitle) {
  apps::mojom::Intent intent;
  intent.share_title = "Today's topic";

  {
    apps::ShareTarget share_target;
    share_target.params.text = "body";
    share_target.params.url = "link";
    EXPECT_EQ(ExtractSharedFields(share_target, intent),
              std::vector<SharedField>());
  }

  {
    apps::ShareTarget share_target;
    share_target.params.title = "subject";
    std::vector<SharedField> expected = {{"subject", "Today's topic"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }
}

TEST(ShareTargetUtils, ExtractText) {
  apps::mojom::Intent intent;
  intent.share_text = "Here's a long message.";

  {
    apps::ShareTarget share_target;
    share_target.params.title = "subject";
    share_target.params.url = "link";
    EXPECT_EQ(ExtractSharedFields(share_target, intent),
              std::vector<SharedField>());
  }

  {
    apps::ShareTarget share_target;
    share_target.params.text = "body";
    std::vector<SharedField> expected = {{"body", "Here's a long message."}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }
}

TEST(ShareTargetUtils, ExtractUrl) {
  apps::mojom::Intent intent;
  // Shared URLs are serialized in share_text.
  intent.share_text = "https://example.com/~me/index.html#part";

  {
    apps::ShareTarget share_target;
    share_target.params.title = "subject";
    share_target.params.text = "body";
    EXPECT_EQ(ExtractSharedFields(share_target, intent),
              std::vector<SharedField>());
  }

  {
    apps::ShareTarget share_target;
    share_target.params.url = "link";
    std::vector<SharedField> expected = {
        {"link", "https://example.com/~me/index.html#part"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }
}

TEST(ShareTargetUtils, ExtractTitleTextUrl) {
  apps::mojom::Intent intent;
  intent.share_title = "Browse";
  intent.share_text =
      "Visit the sites https://example.com/ and https://example.org/";

  {
    apps::ShareTarget share_target;
    share_target.params.title = "subject";
    share_target.params.text = "body";
    share_target.params.url = "link";
    std::vector<SharedField> expected = {
        {"subject", "Browse"},
        {"body", "Visit the sites https://example.com/ and"},
        {"link", "https://example.org/"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }

  {
    apps::ShareTarget share_target;
    share_target.params.title = "subject";
    std::vector<SharedField> expected = {{"subject", "Browse"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }

  {
    apps::ShareTarget share_target;
    share_target.params.text = "body";
    std::vector<SharedField> expected = {
        {"body", "Visit the sites https://example.com/ and"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }

  {
    apps::ShareTarget share_target;
    share_target.params.url = "link";
    std::vector<SharedField> expected = {{"link", "https://example.org/"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }
}

TEST(ShareTargetUtils, SkipEmpty) {
  apps::mojom::Intent intent;
  intent.share_title = "";
  intent.share_text = "";

  {
    apps::ShareTarget share_target;
    share_target.params.title = "subject";
    share_target.params.text = "body";
    share_target.params.url = "link";
    EXPECT_EQ(ExtractSharedFields(share_target, intent),
              std::vector<SharedField>());
  }
}

}  // namespace web_app

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/share_target_utils.h"

#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(ShareTargetUtils, ExtractTitle) {
  apps::Intent intent(apps_util::kIntentActionSend);
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
  apps::Intent intent(apps_util::kIntentActionSend);
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
  apps::Intent intent(apps_util::kIntentActionSend);
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

TEST(ShareTargetUtils, ExtractTextUrl) {
  apps::ShareTarget share_target;
  share_target.params.text = "body";
  share_target.params.url = "link";

  {
    apps::Intent intent(apps_util::kIntentActionSend);
    intent.share_text = "One line\nhttps://example.org/";
    std::vector<SharedField> expected = {{"body", "One line"},
                                         {"link", "https://example.org/"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }

  {
    apps::Intent intent(apps_util::kIntentActionSend);
    intent.share_text = "Two\nlines\nhttps://example.org/";
    std::vector<SharedField> expected = {{"body", "Two\nlines"},
                                         {"link", "https://example.org/"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }

  {
    apps::Intent intent(apps_util::kIntentActionSend);
    intent.share_text = "Many\nmany\nlines https://example.org/";
    std::vector<SharedField> expected = {{"body", "Many\nmany\nlines"},
                                         {"link", "https://example.org/"}};
    EXPECT_EQ(ExtractSharedFields(share_target, intent), expected);
  }
}

TEST(ShareTargetUtils, ExtractTitleTextUrl) {
  apps::Intent intent(apps_util::kIntentActionSend);
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
  apps::Intent intent(apps_util::kIntentActionSend);
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/emoji_page_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::emoji_picker::mojom::EmojiVariant;
using ::emoji_picker::mojom::EmojiVariantPtr;
using ::emoji_picker::mojom::Category::kEmojis;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

class EmojiPageHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_ui_.set_web_contents(
        web_contents_factory_.CreateWebContents(profile_.get()));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  content::TestWebContentsFactory web_contents_factory_;
};

TEST_F(EmojiPageHandlerTest, UpdatesEmojiHistoryInPrefs) {
  mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver;
  EmojiPageHandler handler(std::move(receiver), &web_ui_, nullptr, false, false,
                           kEmojis, "");

  handler.UpdateHistoryInPrefs(kEmojis, {"abc", "xyz"});

  const base::Value::Dict& history =
      profile_->GetPrefs()->GetDict(prefs::kEmojiPickerHistory);
  const base::Value::List* emoji_history = history.FindList("emoji");
  EXPECT_EQ(emoji_history->size(), 2u);
  EXPECT_EQ((*emoji_history)[0].GetDict().Find("text")->GetString(), "abc");
  EXPECT_EQ((*emoji_history)[1].GetDict().Find("text")->GetString(), "xyz");
}

TEST_F(EmojiPageHandlerTest, UpdatesPerferredVariantsInPrefs) {
  mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver;
  EmojiPageHandler handler(std::move(receiver), &web_ui_, nullptr, false, false,
                           kEmojis, "");

  std::vector<EmojiVariantPtr> variants;
  variants.push_back(EmojiVariant::New("abc", "123"));
  variants.push_back(EmojiVariant::New("xyz", "456"));
  handler.UpdatePreferredVariantsInPrefs(std::move(variants));

  const base::Value::Dict& preference =
      profile_->GetPrefs()->GetDict(prefs::kEmojiPickerPreferences);
  const base::Value::Dict* preferred_variants =
      preference.FindDict("preferred_variants");
  EXPECT_EQ(preferred_variants->Find("abc")->GetString(), "123");
  EXPECT_EQ(preferred_variants->Find("xyz")->GetString(), "456");
}

TEST_F(EmojiPageHandlerTest, GetsHistoryFromPrefs) {
  mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver;
  EmojiPageHandler handler(std::move(receiver), &web_ui_, nullptr, false, false,
                           kEmojis, "");
  handler.UpdateHistoryInPrefs(kEmojis, {"abc", "xyz"});

  base::test::TestFuture<const std::vector<std::string>&> future;
  handler.GetHistoryFromPrefs(kEmojis, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ElementsAre("abc", "xyz"));
}

TEST_F(EmojiPageHandlerTest, GetsEmptyHistoryFromEmptyPrefs) {
  mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver;
  EmojiPageHandler handler(std::move(receiver), &web_ui_, nullptr, false, false,
                           kEmojis, "");

  base::test::TestFuture<const std::vector<std::string>&> future;
  handler.GetHistoryFromPrefs(kEmojis, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), IsEmpty());
}

}  // namespace
}  // namespace ash

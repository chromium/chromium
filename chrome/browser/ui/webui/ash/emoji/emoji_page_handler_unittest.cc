// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/emoji_page_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/json/values_util.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
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
using ::emoji_picker::mojom::HistoryItem;
using ::emoji_picker::mojom::HistoryItemPtr;
using ::emoji_picker::mojom::Category::kEmojis;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;

base::Time TimeFromSeconds(int64_t seconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(seconds));
}

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

  std::vector<HistoryItemPtr> history;
  history.push_back(HistoryItem::New("abc", TimeFromSeconds(10)));
  history.push_back(HistoryItem::New("xyz", TimeFromSeconds(5)));
  handler.UpdateHistoryInPrefs(kEmojis, std::move(history));

  const base::Value::List* emoji_history =
      profile_->GetPrefs()
          ->GetDict(prefs::kEmojiPickerHistory)
          .FindList("emoji");
  EXPECT_EQ(emoji_history->size(), 2u);

  auto& item0 = (*emoji_history)[0].GetDict();
  EXPECT_EQ(item0.Find("text")->GetString(), "abc");
  EXPECT_EQ(base::ValueToTime(item0.Find("timestamp")), TimeFromSeconds(10));

  auto& item1 = (*emoji_history)[1].GetDict();
  EXPECT_EQ(item1.Find("text")->GetString(), "xyz");
  EXPECT_EQ(base::ValueToTime(item1.Find("timestamp")), TimeFromSeconds(5));
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
  std::vector<HistoryItemPtr> history;
  history.push_back(HistoryItem::New("abc", TimeFromSeconds(10)));
  history.push_back(HistoryItem::New("xyz", TimeFromSeconds(5)));
  handler.UpdateHistoryInPrefs(kEmojis, std::move(history));

  base::test::TestFuture<std::vector<HistoryItemPtr>> future;
  handler.GetHistoryFromPrefs(kEmojis, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(
      future.Get(),
      ElementsAre(Pointee(AllOf(Field("text", &HistoryItem::emoji, "abc"),
                                Field("timestamp", &HistoryItem::timestamp,
                                      TimeFromSeconds(10)))),
                  Pointee(AllOf(Field("text", &HistoryItem::emoji, "xyz"),
                                Field("timestamp", &HistoryItem::timestamp,
                                      TimeFromSeconds(5))))));
}

TEST_F(EmojiPageHandlerTest, GetsEmptyHistoryFromEmptyPrefs) {
  mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver;
  EmojiPageHandler handler(std::move(receiver), &web_ui_, nullptr, false, false,
                           kEmojis, "");

  base::test::TestFuture<std::vector<HistoryItemPtr>> future;
  handler.GetHistoryFromPrefs(kEmojis, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), IsEmpty());
}

}  // namespace
}  // namespace ash

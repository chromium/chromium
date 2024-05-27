// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/emoji_page_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

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
                           emoji_picker::mojom::Category::kEmojis, "");

  handler.UpdateHistoryInPrefs(emoji_picker::mojom::Category::kEmojis,
                               {"abc", "xyz"});

  const base::Value::Dict& history =
      profile_->GetPrefs()->GetDict(ash::prefs::kEmojiPickerHistory);
  const base::Value::List* emoji_history =
      history.FindListByDottedPath("emoji");
  EXPECT_EQ(emoji_history->size(), 2u);
  EXPECT_EQ((*emoji_history)[0].GetDict().Find("text")->GetString(), "abc");
  EXPECT_EQ((*emoji_history)[1].GetDict().Find("text")->GetString(), "xyz");
}

}  // namespace
}  // namespace ash

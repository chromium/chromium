// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delgate.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/content_settings/core/common/content_settings_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ImageType = ContentSettingImageModel::ImageType;

class TestDelegate : public ContentSettingImageViewDelegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  bool ShouldHideContentSettingImage() override { return false; }
  content::WebContents* GetContentSettingWebContents() override {
    return web_contents_;
  }
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override {
    return nullptr;
  }

  void SetWebContents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  // Safe, this delegate is destroyed in TearDown(), before the WebContents.
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

class FakeContentSettingImageModel : public ContentSettingSimpleImageModel {
 public:
  FakeContentSettingImageModel(
      ImageType image_type,
      ContentSettingsType content_type,
      bool image_type_should_notify_accessibility = false)
      : ContentSettingSimpleImageModel(
            image_type,
            content_type,
            /*image_type_should_notify_accessibility=*/
            image_type_should_notify_accessibility) {}

  bool UpdateAndGetVisibility(content::WebContents* web_contents) override {
    return visible_;
  }

  void set_visible(bool visible) { visible_ = visible; }
  void set_blocked(bool blocked) { SetIcon(content_type(), blocked); }
  using ContentSettingImageModel::set_accessibility_string_id;
  using ContentSettingImageModel::set_explanatory_string_id;
  using ContentSettingImageModel::set_should_auto_open_bubble;
  using ContentSettingImageModel::set_tooltip;

 private:
  bool visible_ = false;
};

class MockContentSettingImageModel : public FakeContentSettingImageModel {
 public:
  MockContentSettingImageModel(ImageType image_type,
                               ContentSettingsType content_type)
      : FakeContentSettingImageModel(image_type, content_type) {}

  MOCK_METHOD(std::unique_ptr<ContentSettingBubbleModel>,
              CreateBubbleModelImpl,
              (ContentSettingBubbleModel::Delegate*, content::WebContents*),
              (override));
};

class WebUIContentSettingImageControlTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    delegate_ = std::make_unique<TestDelegate>();
    delegate_->SetWebContents(web_contents());
    control_ =
        std::make_unique<WebUIContentSettingImageControl>(delegate_.get());
  }

  void TearDown() override {
    control_.reset();
    delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<TestDelegate> delegate_;
  std::unique_ptr<WebUIContentSettingImageControl> control_;
};

TEST_F(WebUIContentSettingImageControlTest, ProcessContentSettingState_Empty) {
  control_->InitForTesting({});
  auto state = control_->ProcessContentSettingState(web_contents());
  EXPECT_EQ(0u, state.size());
}

TEST_F(WebUIContentSettingImageControlTest,
       ProcessContentSettingState_Mapping) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  auto cookies_model_ptr = std::make_unique<FakeContentSettingImageModel>(
      ImageType::kCookies, ContentSettingsType::COOKIES);
  auto* cookies_model = cookies_model_ptr.get();
  models.push_back(std::move(cookies_model_ptr));

  control_->InitForTesting(std::move(models));

  // Test hidden state.
  cookies_model->set_visible(false);
  auto state = control_->ProcessContentSettingState(web_contents());
  EXPECT_EQ(0u, state.size());

  // Test visible and blocked state.
  cookies_model->set_visible(true);
  cookies_model->set_blocked(true);
  cookies_model->set_tooltip(u"Cookie Tooltip");
  cookies_model->set_explanatory_string_id(
      IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT);
  cookies_model->set_accessibility_string_id(IDS_BLOCKED_POPUPS_TOOLTIP);

  state = control_->ProcessContentSettingState(web_contents());
  ASSERT_EQ(1u, state.size());
  const auto& cookie_state = state[0];

  EXPECT_EQ(ImageType::kCookies, cookie_state->type);
  EXPECT_TRUE(cookie_state->is_blocked);
  EXPECT_EQ(u"Cookie Tooltip", cookie_state->tooltip);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT),
            cookie_state->explanatory_string);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_TOOLTIP),
            cookie_state->accessibility_string);
}

TEST_F(WebUIContentSettingImageControlTest,
       ProcessContentSettingState_Mapping_Multiple) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  auto cookies_model_ptr = std::make_unique<FakeContentSettingImageModel>(
      ImageType::kCookies, ContentSettingsType::COOKIES);
  auto* cookies_model = cookies_model_ptr.get();
  models.push_back(std::move(cookies_model_ptr));
  auto popup_model_ptr = std::make_unique<FakeContentSettingImageModel>(
      ImageType::kPopups, ContentSettingsType::POPUPS);
  auto* popup_model = popup_model_ptr.get();
  models.push_back(std::move(popup_model_ptr));

  control_->InitForTesting(std::move(models));

  // Test hidden state.
  cookies_model->set_visible(false);
  popup_model->set_visible(true);
  auto state = control_->ProcessContentSettingState(web_contents());
  EXPECT_EQ(1u, state.size());

  // Test visible and blocked state.
  cookies_model->set_visible(true);
  cookies_model->set_blocked(true);
  cookies_model->set_tooltip(u"Cookie Tooltip");

  popup_model->set_visible(true);
  popup_model->set_blocked(false);
  popup_model->set_tooltip(u"Popup Tooltip");

  state = control_->ProcessContentSettingState(web_contents());
  ASSERT_EQ(2u, state.size());
  const auto& cookie_state = state[0];
  const auto& popup_state = state[1];

  EXPECT_EQ(ImageType::kCookies, cookie_state->type);
  EXPECT_TRUE(cookie_state->is_blocked);
  EXPECT_EQ(u"Cookie Tooltip", cookie_state->tooltip);

  EXPECT_EQ(ImageType::kPopups, popup_state->type);
  EXPECT_FALSE(popup_state->is_blocked);
  EXPECT_EQ(u"Popup Tooltip", popup_state->tooltip);
}

TEST_F(WebUIContentSettingImageControlTest, ShowContentSettingsBubble) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  auto cookies_model_ptr = std::make_unique<MockContentSettingImageModel>(
      ImageType::kCookies, ContentSettingsType::COOKIES);
  auto* cookies_model = cookies_model_ptr.get();
  cookies_model->set_visible(true);
  models.push_back(std::move(cookies_model_ptr));

  auto popups_model_ptr = std::make_unique<MockContentSettingImageModel>(
      ImageType::kPopups, ContentSettingsType::POPUPS);
  auto* popups_model = popups_model_ptr.get();
  popups_model->set_visible(true);
  models.push_back(std::move(popups_model_ptr));

  control_->InitForTesting(std::move(models));

  // The cookies model should be called.
  EXPECT_CALL(
      *cookies_model,
      CreateBubbleModelImpl(delegate_->GetContentSettingBubbleModelDelegate(),
                            web_contents()))
      .WillOnce(testing::Return(
          testing::ByMove(std::unique_ptr<ContentSettingBubbleModel>())));

  // The popups model should NOT be called.
  EXPECT_CALL(*popups_model, CreateBubbleModelImpl(testing::_, testing::_))
      .Times(0);

  control_->ShowContentSettingsBubble(ImageType::kCookies, base::DoNothing());
}

TEST_F(WebUIContentSettingImageControlTest, AutoOpenBubble) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  auto cookies_model_ptr = std::make_unique<MockContentSettingImageModel>(
      ImageType::kCookies, ContentSettingsType::COOKIES);
  auto* cookies_model = cookies_model_ptr.get();
  cookies_model->set_visible(true);
  cookies_model->set_should_auto_open_bubble(true);
  models.push_back(std::move(cookies_model_ptr));

  auto popups_model_ptr = std::make_unique<MockContentSettingImageModel>(
      ImageType::kPopups, ContentSettingsType::POPUPS);
  auto* popups_model = popups_model_ptr.get();
  popups_model->set_visible(true);
  models.push_back(std::move(popups_model_ptr));

  control_->InitForTesting(std::move(models));

  // The cookies model should be called.
  EXPECT_CALL(
      *cookies_model,
      CreateBubbleModelImpl(delegate_->GetContentSettingBubbleModelDelegate(),
                            web_contents()))
      .WillOnce(testing::Return(
          testing::ByMove(std::unique_ptr<ContentSettingBubbleModel>())));

  // The popups model should NOT be called.
  EXPECT_CALL(*popups_model, CreateBubbleModelImpl(testing::_, testing::_))
      .Times(0);

  auto state = control_->ProcessContentSettingState(web_contents());
  EXPECT_EQ(2u, state.size());
}

TEST_F(WebUIContentSettingImageControlTest, AccessibilityAnnouncement) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  auto cookies_model_ptr = std::make_unique<FakeContentSettingImageModel>(
      ImageType::kCookies, ContentSettingsType::COOKIES,
      /*image_type_should_notify_accessibility=*/true);
  auto* cookies_model = cookies_model_ptr.get();
  cookies_model->set_visible(true);
  cookies_model->set_accessibility_string_id(IDS_BLOCKED_POPUPS_TOOLTIP);
  models.push_back(std::move(cookies_model_ptr));

  MockWebUIToolbarControlDelegate webui_delegate;
  control_->InitForTesting(std::move(models), &webui_delegate);

  std::u16string name = l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_TOOLTIP);
  std::u16string expected_announcement = l10n_util::GetStringFUTF16(
      IDS_A11Y_INDICATORS_ANNOUNCEMENT, name,
      l10n_util::GetStringUTF16(IDS_A11Y_OMNIBOX_CHIP_HINT));

  EXPECT_CALL(webui_delegate, AnnounceAlert(expected_announcement)).Times(1);

  auto state = control_->ProcessContentSettingState(web_contents());
  ASSERT_EQ(1u, state.size());
}

TEST_F(WebUIContentSettingImageControlTest, AnimationAnnouncement) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  auto popups_model_ptr = std::make_unique<FakeContentSettingImageModel>(
      ImageType::kPopups, ContentSettingsType::POPUPS,
      /*image_type_should_notify_accessibility=*/false);
  auto* popups_model = popups_model_ptr.get();
  popups_model->set_visible(true);
  popups_model->set_explanatory_string_id(IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT);
  models.push_back(std::move(popups_model_ptr));

  MockWebUIToolbarControlDelegate webui_delegate;
  control_->InitForTesting(std::move(models), &webui_delegate);

  std::u16string expected_announcement =
      l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT);

  EXPECT_CALL(webui_delegate, AnnounceAlert(expected_announcement)).Times(1);

  auto state = control_->ProcessContentSettingState(web_contents());
  ASSERT_EQ(1u, state.size());
  EXPECT_TRUE(state[0]->should_run_animation);
}

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

using ::testing::_;

class TestDelegate : public IconLabelBubbleView::Delegate,
                     public ContentSettingImageViewDelegate {
 public:
  explicit TestDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override {
    return SK_ColorBLACK;
  }
  SkColor GetIconLabelBubbleBackgroundColor() const override {
    return SK_ColorWHITE;
  }

  // ContentSettingImageViewDelegate:
  bool ShouldHideContentSettingImage() override { return false; }
  content::WebContents* GetContentSettingWebContents() override {
    return web_contents_;
  }
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override {
    return nullptr;
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

class FakeContentSettingImageModel : public ContentSettingSimpleImageModel {
 public:
  FakeContentSettingImageModel()
      : ContentSettingSimpleImageModel(
            ImageType::kNotifications,
            ContentSettingsType::NOTIFICATIONS,
            /*image_type_should_notify_accessibility=*/true) {
    SetIcon(ContentSettingsType::NOTIFICATIONS, /*blocked=*/false);
    set_accessibility_string_id(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
  }

  void set_visible(bool visible) { visible_ = visible; }
  void set_label_string_id(int string_id) { label_string_id_ = string_id; }
  void set_promo_key(std::optional<std::string> promo_key) {
    promo_key_ = std::move(promo_key);
  }

 protected:
  bool UpdateAndGetVisibility(content::WebContents* web_contents) override {
    set_explanatory_string_id(0);
    if (!visible_) {
      return false;
    }
    set_explanatory_string_id(label_string_id_);
    if (promo_key_) {
      set_promo_params(user_education::FeaturePromoParams(
          feature_engagement::kIPHPwaQuietNotificationFeature, *promo_key_));
    }
    return true;
  }

 private:
  bool visible_ = false;
  int label_string_id_ = 0;
  std::optional<std::string> promo_key_;
};

class ContentSettingImageViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    delegate_ = std::make_unique<TestDelegate>(web_contents_.get());
    browser_window_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    user_education_ =
        std::make_unique<testing::NiceMock<MockBrowserUserEducationInterface>>(
            browser_window_.get());

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    container_ = widget_->SetContentsView(std::make_unique<views::View>());
    widget_->Show();
  }

  void TearDown() override {
    container_ = nullptr;
    widget_.reset();
    user_education_.reset();
    browser_window_.reset();
    delegate_.reset();
    web_contents_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::pair<ContentSettingImageView*, FakeContentSettingImageModel*>
  AddContentSettingImageView() {
    auto model = std::make_unique<FakeContentSettingImageModel>();
    FakeContentSettingImageModel* model_ptr = model.get();
    ContentSettingImageView* view =
        container_->AddChildView(std::make_unique<ContentSettingImageView>(
            std::move(model), delegate_.get(), delegate_.get(),
            browser_window_.get(), gfx::FontList()));
    return {view, model_ptr};
  }

 protected:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestDelegate> delegate_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_;
  std::unique_ptr<MockBrowserUserEducationInterface> user_education_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> container_ = nullptr;
};

// When an icon has a promo and no explanatory label, the promo is shown
// immediately in Update() and not requested again until the icon hides.
TEST_F(ContentSettingImageViewTest, PromoShownImmediatelyWithoutAnimation) {
  auto [view, model] = AddContentSettingImageView();
  model->set_visible(true);
  model->set_promo_key("test_app_id");

  EXPECT_CALL(*user_education_, MaybeShowFeaturePromo(_))
      .WillOnce([](user_education::FeaturePromoParams params) {
        EXPECT_EQ(&params.feature.get(),
                  &feature_engagement::kIPHPwaQuietNotificationFeature);
        EXPECT_EQ(params.key, "test_app_id");
        return true;
      });
  view->Update();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
  testing::Mock::VerifyAndClearExpectations(user_education_.get());

  // Subsequent updates while the icon stays visible do not re-trigger the
  // promo.
  EXPECT_CALL(*user_education_, MaybeShowFeaturePromo(_)).Times(0);
  view->Update();
  testing::Mock::VerifyAndClearExpectations(user_education_.get());

  // Hiding and re-showing the icon allows the promo to be requested again.
  model->set_visible(false);
  view->Update();
  EXPECT_FALSE(view->GetVisible());

  model->set_visible(true);
  EXPECT_CALL(*user_education_, MaybeShowFeaturePromo(_)).Times(1);
  view->Update();
}

// When an icon has both an explanatory label animation and a promo, the promo
// waits for the label animation to finish (and is not prematurely triggered by
// a second view sharing the same WebContents, such as a PWA's hidden location
// bar).
TEST_F(ContentSettingImageViewTest, PromoDeferredUntilAnimationEnds) {
  gfx::ScopedAnimationDurationScaleMode normal_duration(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
  auto reduced_motion_reset =
      gfx::AnimationTestApi::SetPrefersReducedMotionForTesting(false);

  auto [primary_view, primary_model] = AddContentSettingImageView();
  auto [secondary_view, secondary_model] = AddContentSettingImageView();
  for (FakeContentSettingImageModel* model : {primary_model, secondary_model}) {
    model->set_visible(true);
    model->set_label_string_id(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
    model->set_promo_key("test_app_id");
  }

  EXPECT_CALL(*user_education_, MaybeShowFeaturePromo(_)).Times(0);
  primary_view->Update();
  secondary_view->Update();
  EXPECT_TRUE(primary_view->is_animating_label());
  EXPECT_TRUE(primary_view->ShouldShowLabel());
  testing::Mock::VerifyAndClearExpectations(user_education_.get());

  EXPECT_CALL(*user_education_, MaybeShowFeaturePromo(_))
      .WillOnce([](user_education::FeaturePromoParams params) {
        EXPECT_EQ(&params.feature.get(),
                  &feature_engagement::kIPHPwaQuietNotificationFeature);
        EXPECT_EQ(params.key, "test_app_id");
        return true;
      });
  gfx::AnimationTestApi animation(&primary_view->slide_animation_for_testing());
  base::TimeTicks now = base::TimeTicks::Now();
  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(5));
  EXPECT_FALSE(primary_view->is_animating_label());
  EXPECT_FALSE(primary_view->ShouldShowLabel());
}

// When animations are disabled on the view, the promo is shown immediately in
// Update() even if the model specifies an explanatory label.
TEST_F(ContentSettingImageViewTest,
       PromoShownImmediatelyWhenAnimationDisabled) {
  auto [view, model] = AddContentSettingImageView();
  view->disable_animation();
  model->set_visible(true);
  model->set_label_string_id(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
  model->set_promo_key("test_app_id");

  EXPECT_CALL(*user_education_, MaybeShowFeaturePromo(_)).Times(1);
  view->Update();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

}  // namespace

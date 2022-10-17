// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_view.h"

#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/test_platform_native_widget.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace {
const char kTestURL[] = "about:blank";
}  // namespace

class PriceTrackingViewTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.context = GetContext();
    widget_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget_->Init(std::move(widget_params));
  }

  void TearDown() override {
    anchor_widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories = {
        {BookmarkModelFactory::GetInstance(),
         BookmarkModelFactory::GetDefaultFactory()},
        {commerce::ShoppingServiceFactory::GetInstance(),
         base::BindRepeating([](content::BrowserContext* context) {
           return commerce::MockShoppingService::Build();
         })}};
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    return factories;
  }

  void SetUpDependencies() {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kTestURL),
                                  std::u16string());

    commerce::AddProductBookmark(bookmark_model, u"title", GURL(kTestURL), 0,
                                 false);
  }

  raw_ptr<PriceTrackingView> CreateViewAndShow(bool is_price_track_enabled) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    const auto valid_product_image =
        gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    auto price_tracking_View = std::make_unique<PriceTrackingView>(
        profile(), GURL(kTestURL), *valid_product_image.ToImageSkia(),
        is_price_track_enabled);
    price_tracking_View_ =
        anchor_widget_->SetContentsView(std::move(price_tracking_View));
    anchor_widget_->Show();
    return price_tracking_View_;
  }

  void ClickToggle() {
    auto toggle_button = price_tracking_View_->toggle_button_;
    gfx::Point toggle_center = toggle_button->GetLocalBounds().CenterPoint();
    gfx::Point root_center = toggle_center;
    views::View::ConvertPointToWidget(price_tracking_View_, &root_center);
    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, toggle_center,
                                 root_center, base::TimeTicks(),
                                 ui::EF_LEFT_MOUSE_BUTTON, 0);

    toggle_button->OnMousePressed(pressed_event);

    ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, toggle_center,
                                  root_center, base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
    toggle_button->OnMouseReleased(released_event);
    task_environment()->RunUntilIdle();
  }

  void VerifyToggleState(bool expected_toggle_on) {
    EXPECT_EQ(price_tracking_View_->IsToggleOn(), expected_toggle_on);

    if (expected_toggle_on) {
      EXPECT_EQ(price_tracking_View_->toggle_button_->GetAccessibleName(),
                l10n_util::GetStringUTF16(
                    IDS_PRICE_TRACKING_UNTRACK_PRODUCT_ACCESSIBILITY));
    } else {
      EXPECT_EQ(price_tracking_View_->toggle_button_->GetAccessibleName(),
                l10n_util::GetStringUTF16(
                    IDS_PRICE_TRACKING_TRACK_PRODUCT_ACCESSIBILITY));
    }
  }

  void VerifyBodyMessage(std::u16string expected_message) {
    EXPECT_EQ(price_tracking_View_->body_label_->GetText(), expected_message);
  }

 protected:
  base::UserActionTester user_action_tester_;

 private:
  views::UniqueWidgetPtr anchor_widget_;
  raw_ptr<PriceTrackingView> price_tracking_View_;
};

TEST_F(PriceTrackingViewTest, InitialPriceTrackEnabled) {
  const bool enabled = true;
  CreateViewAndShow(enabled);
  VerifyToggleState(enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION));
}

TEST_F(PriceTrackingViewTest, InitialPriceTrackDisabled) {
  const bool enabled = false;
  CreateViewAndShow(enabled);
  VerifyToggleState(enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION));
}

TEST_F(PriceTrackingViewTest, ToggleSuccessed) {
  SetUpDependencies();

  const bool initial_enabled = false;
  CreateViewAndShow(initial_enabled);
  VerifyToggleState(initial_enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION));

  static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile()))
      ->SetSubscribeCallbackValue(true);

  ClickToggle();
  VerifyToggleState(!initial_enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION));
}

TEST_F(PriceTrackingViewTest, ToggleFailed) {
  SetUpDependencies();

  const bool initial_enabled = false;
  CreateViewAndShow(initial_enabled);
  VerifyToggleState(initial_enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION));

  static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile()))
      ->SetSubscribeCallbackValue(false);

  ClickToggle();
  VerifyToggleState(initial_enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_OMNIBOX_TRACK_PRICE_DIALOG_ERROR_DESCRIPTION));
}

TEST_F(PriceTrackingViewTest, ToggleRecordTracked) {
  SetUpDependencies();

  const bool initial_enabled = false;
  CreateViewAndShow(initial_enabled);
  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "Commerce.PriceTracking.BookmarkDialogPriceTrackViewTrackedPrice"),
      0);
  ClickToggle();

  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "Commerce.PriceTracking.BookmarkDialogPriceTrackViewTrackedPrice"),
      1);
}

TEST_F(PriceTrackingViewTest, ToggleRecordUntracked) {
  SetUpDependencies();

  const bool initial_enabled = true;
  CreateViewAndShow(initial_enabled);
  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "Commerce.PriceTracking.BookmarkDialogPriceTrackViewUntrackedPrice"),
      0);
  ClickToggle();

  EXPECT_EQ(
      user_action_tester_.GetActionCount(
          "Commerce.PriceTracking.BookmarkDialogPriceTrackViewUntrackedPrice"),
      1);
}

TEST_F(PriceTrackingViewTest, EmailTurnedOff) {
  profile()->GetPrefs()->SetBoolean(commerce::kPriceEmailNotificationsEnabled,
                                    false);
  const bool enabled = false;
  CreateViewAndShow(enabled);
  VerifyToggleState(enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION_EMAIL_OFF));
}

TEST_F(PriceTrackingViewTest, EmailTurnedOn) {
  profile()->GetPrefs()->SetBoolean(commerce::kPriceEmailNotificationsEnabled,
                                    true);
  const bool enabled = false;
  CreateViewAndShow(enabled);
  VerifyToggleState(enabled);
  VerifyBodyMessage(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION));
}

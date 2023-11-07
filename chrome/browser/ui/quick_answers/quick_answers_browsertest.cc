// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/quick_answers/quick_answers_browsertest_base.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

constexpr char kTestQuery[] = "test";
constexpr int kCursorXToOverlapWithANotification = 630;
constexpr int kCursorYToOverlapWithANotification = 400;

constexpr char kTestNotificationId[] = "id";
constexpr char16_t kTestNotificationTitle[] = u"title";
constexpr char16_t kTestNotificationMessage[] = u"message";
constexpr char16_t kTestNotificationDisplaySource[] = u"display-source";
constexpr char kTestNotificationOriginUrl[] = "https://example.com/";

constexpr char16_t kSourceText[] = u"prodotto";
constexpr char16_t kTranslatedText[] = u"product";

constexpr int kFakeImageWidth = 300;
constexpr int kFakeImageHeight = 300;

constexpr int kAnimationCompletionPollingInterval = 50;  // milliseconds

constexpr int kQuickAnswersResultTypeIconSizeDip = 12;
constexpr int kRichAnswersResultTypeIconSizeDip = 16;

gfx::Image CreateFakeImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kFakeImageWidth, kFakeImageHeight);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

void CheckAnimationEnded(views::Widget* widget,
                         gfx::Rect* rect,
                         base::RunLoop* run_loop) {
  gfx::Rect current_rect = widget->GetWindowBoundsInScreen();

  if (current_rect == *rect) {
    run_loop->Quit();
    return;
  }

  *rect = current_rect;
}

void WaitAnimationCompletion(views::Widget* widget) {
  base::RunLoop run_loop;
  base::RepeatingTimer timer;
  gfx::Rect rect;

  timer.Start(
      FROM_HERE, base::Milliseconds(kAnimationCompletionPollingInterval),
      base::BindRepeating(&CheckAnimationEnded, widget, &rect, &run_loop));
  run_loop.Run();
}

}  // namespace

class QuickAnswersBrowserTest : public QuickAnswersBrowserTestBase {
 protected:
  // This simulates a behavior where a user enables QuickAnswers from Settings.
  void SetQuickAnswersEnabled(bool enabled) {
    chrome_test_utils::GetProfile(this)->GetPrefs()->SetBoolean(
        prefs::kQuickAnswersEnabled, enabled);
  }

  void SendTestImageNotification() {
    message_center::RichNotificationData rich_notification_data;
    rich_notification_data.image = CreateFakeImage();
    rich_notification_data.never_timeout = true;

    message_center::Notification notification(
        message_center::NotificationType::NOTIFICATION_TYPE_IMAGE,
        kTestNotificationId, kTestNotificationTitle, kTestNotificationMessage,
        /*icon=*/ui::ImageModel(), kTestNotificationDisplaySource,
        GURL(kTestNotificationOriginUrl), message_center::NotifierId(),
        rich_notification_data,
        base::MakeRefCounted<message_center::NotificationDelegate>());

    NotificationDisplayService::GetForProfile(
        chrome_test_utils::GetProfile(this))
        ->Display(NotificationHandler::Type::TRANSIENT, notification,
                  /*metadata=*/nullptr);
  }
};

IN_PROC_BROWSER_TEST_F(QuickAnswersBrowserTest,
                       QuickAnswersViewAboveNotification) {
  SetQuickAnswersEnabled(true);

  views::NamedWidgetShownWaiter quick_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), QuickAnswersView::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenu(params);

  views::Widget* quick_answers_view_widget =
      quick_answers_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(quick_answers_view_widget != nullptr);

  views::NamedWidgetShownWaiter message_popup_widget_waiter(
      views::test::AnyWidgetTestPasskey(),
      ash::AshMessagePopupCollection::kMessagePopupWidgetName);

  SendTestImageNotification();

  views::Widget* message_popup_widget =
      message_popup_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(message_popup_widget != nullptr);

  // The notification is animating. Wait the animation completion before
  // checking the bounds.
  WaitAnimationCompletion(message_popup_widget);

  // Make sure that `QuickAnswersView` overlaps with the notification.
  ASSERT_FALSE(
      gfx::IntersectRects(message_popup_widget->GetWindowBoundsInScreen(),
                          quick_answers_view_widget->GetWindowBoundsInScreen())
          .IsEmpty());

  // TODO(b/239716419): Quick answers UI should be above the notification.
  EXPECT_TRUE(message_popup_widget->IsStackedAbove(
      quick_answers_view_widget->GetNativeView()));
}

IN_PROC_BROWSER_TEST_F(QuickAnswersBrowserTest,
                       UserConsentViewAboveNotification) {
  views::NamedWidgetShownWaiter user_consent_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), UserConsentView::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenu(params);

  views::Widget* user_consent_view_widget =
      user_consent_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(user_consent_view_widget != nullptr);

  views::NamedWidgetShownWaiter message_popup_widget_waiter(
      views::test::AnyWidgetTestPasskey(),
      ash::AshMessagePopupCollection::kMessagePopupWidgetName);

  SendTestImageNotification();

  views::Widget* message_popup_widget =
      message_popup_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(message_popup_widget != nullptr);

  // The notification is animating. Wait the animation completion before
  // checking the bounds.
  WaitAnimationCompletion(message_popup_widget);

  // Make sure that `UserConsentView` overlaps with the notification.
  ASSERT_FALSE(
      gfx::IntersectRects(message_popup_widget->GetWindowBoundsInScreen(),
                          user_consent_view_widget->GetWindowBoundsInScreen())
          .IsEmpty());

  // TODO(b/239716419): Quick answers UI should be above the notification.
  EXPECT_TRUE(message_popup_widget->IsStackedAbove(
      user_consent_view_widget->GetNativeView()));
}

class RichAnswersBrowserTest : public QuickAnswersBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    QuickAnswersBrowserTestBase::SetUpOnMainThread();
    SetQuickAnswersEnabled(true);
  }

  // Simulate a valid QuickAnswer translation response.
  std::unique_ptr<QuickAnswersSession> CreateQuickAnswerTranslationResponse() {
    std::unique_ptr<quick_answers::QuickAnswer> quick_answer =
        std::make_unique<quick_answers::QuickAnswer>();
    quick_answer->result_type = ResultType::kTranslationResult;
    quick_answer->title.push_back(
        std::make_unique<quick_answers::QuickAnswerText>(
            l10n_util::GetStringFUTF8(IDS_QUICK_ANSWERS_TRANSLATION_TITLE_TEXT,
                                      kSourceText, u"Italian")));
    quick_answer->first_answer_row.push_back(
        std::make_unique<quick_answers::QuickAnswerResultText>(
            base::UTF16ToUTF8(kTranslatedText)));

    std::unique_ptr<TranslationResult> translation_result =
        std::make_unique<TranslationResult>();
    translation_result->text_to_translate = kSourceText;
    translation_result->translated_text = kTranslatedText;
    translation_result->target_locale = "en";
    translation_result->source_locale = "it";

    std::unique_ptr<QuickAnswersSession> quick_answers_session =
        std::make_unique<QuickAnswersSession>();
    quick_answers_session->quick_answer = std::move(quick_answer);
    quick_answers_session->structured_result =
        std::make_unique<StructuredResult>();
    quick_answers_session->structured_result->translation_result =
        std::move(translation_result);

    return quick_answers_session;
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      chromeos::features::kQuickAnswersRichCard};
};

IN_PROC_BROWSER_TEST_F(RichAnswersBrowserTest,
                       RichAnswersDismissedOnOutOfBoundsClick) {
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());

  views::NamedWidgetShownWaiter quick_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), QuickAnswersView::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenu(params);

  views::Widget* quick_answers_view_widget =
      quick_answers_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(quick_answers_view_widget != nullptr);

  // Simulate having received a valid QuickAnswer response.
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerTranslationResponse());

  // Click on the quick answers view to trigger the rich answers view.
  views::NamedWidgetShownWaiter rich_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), RichAnswersView::kWidgetName);
  event_generator.MoveMouseTo(
      quick_answers_view_widget->GetWindowBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  // Check that the quick answers view closes when the rich answers view shows.
  views::Widget* rich_answers_view_widget =
      rich_answers_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(quick_answers_view_widget->IsClosed());
  ASSERT_TRUE(rich_answers_view_widget != nullptr);
  ASSERT_TRUE(controller()->GetVisibilityForTesting() ==
              QuickAnswersVisibility::kRichAnswersVisible);

  // Click outside the rich answers view window bounds to dismiss it.
  gfx::Rect rich_answers_bounds =
      rich_answers_view_widget->GetWindowBoundsInScreen();
  event_generator.MoveMouseTo(
      gfx::Point(rich_answers_bounds.x() / 2, rich_answers_bounds.y() / 2));
  event_generator.ClickLeftButton();

  // Check that the rich answers view is dismissed.
  ASSERT_TRUE(rich_answers_view_widget->IsClosed());
}

IN_PROC_BROWSER_TEST_F(RichAnswersBrowserTest,
                       RichAnswersNotTriggeredOnInvalidResult) {
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());

  views::NamedWidgetShownWaiter quick_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), QuickAnswersView::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenu(params);

  views::Widget* quick_answers_view_widget =
      quick_answers_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(quick_answers_view_widget != nullptr);

  // Click on the quick answers view. This should not trigger the
  // rich answers view since no valid QuickAnswer result is provided.
  views::NamedWidgetShownWaiter rich_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), RichAnswersView::kWidgetName);
  event_generator.MoveMouseTo(
      quick_answers_view_widget->GetWindowBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  // Check that all quick answers views are closed.
  ASSERT_TRUE(quick_answers_view_widget->IsClosed());
  ASSERT_TRUE(controller()->GetVisibilityForTesting() ==
              QuickAnswersVisibility::kClosed);
}

IN_PROC_BROWSER_TEST_F(RichAnswersBrowserTest,
                       CorrespondingResultTypeIconShown) {
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());

  views::NamedWidgetShownWaiter quick_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), QuickAnswersView::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenu(params);

  views::Widget* quick_answers_view_widget =
      quick_answers_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(quick_answers_view_widget != nullptr);

  // Simulate having received a valid QuickAnswer response.
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerTranslationResponse());

  // Check that the shown result type icon on the QuickAnswersView
  // correctly corresponds to the Quick Answers result type.
  quick_answers::QuickAnswersView* quick_answers_view =
      static_cast<quick_answers::QuickAnswersView*>(
          quick_answers_view_widget->GetContentsView());
  ui::ImageModel expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerTranslationIcon, cros_tokens::kCrosSysSystemBaseElevated,
      /*icon_size=*/kQuickAnswersResultTypeIconSizeDip);
  ASSERT_TRUE(quick_answers_view->GetIconImageModelForTesting() ==
              expected_image_model);

  // Click on the quick answers view to trigger the rich answers view.
  views::NamedWidgetShownWaiter rich_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), RichAnswersView::kWidgetName);
  event_generator.MoveMouseTo(
      quick_answers_view_widget->GetWindowBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
  views::Widget* rich_answers_view_widget =
      rich_answers_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(rich_answers_view_widget != nullptr);

  // Check that the shown result type icon on the RichAnswersView
  // correctly corresponds to the Quick Answers result type.
  RichAnswersView* rich_answers_view = static_cast<RichAnswersView*>(
      rich_answers_view_widget->GetContentsView());
  expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerTranslationIcon, cros_tokens::kCrosSysSystemBaseElevated,
      /*icon_size=*/kRichAnswersResultTypeIconSizeDip);
  ASSERT_TRUE(rich_answers_view->GetIconImageModelForTesting() ==
              expected_image_model);
}

}  // namespace quick_answers

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_browsertest_base.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/mock_quick_answers_client.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
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

// Definition result.
constexpr char16_t kQueryText[] = u"indefinite";
constexpr char16_t kPhoneticsText[] = u"inˈdef(ə)nət";
constexpr char kPhoneticsAudioUrlWithProtocol[] = "https://example.com/audio";
constexpr char kDefinitionText[] =
    "lasting for an unknown or unstated length of time.";
constexpr char kWordClassText[] = "adjective";
constexpr char kSampleSentenceText[] = "they may face indefinite detention";
constexpr char kFirstSynonymText[] = "unknown";
constexpr char kSecondSynonymText[] = "indeterminate";
constexpr char kThirdSynonymText[] = "unspecified";
constexpr char kSubsenseDefinitionText[] =
    "not clearly expressed or defined; vague.";
constexpr char kSubsenseSampleSentenceText[] =
    "their status remains indefinite";
constexpr char kSubsenseSynonymText[] = "vague";

// Translation result.
constexpr char16_t kSourceText[] = u"prodotto";
constexpr char16_t kTranslatedText[] = u"product";

// Unit conversion result.
constexpr char16_t kSourceValueText[] = u"20";
constexpr char16_t kSourceUnitText[] = u"feet";
constexpr char kConversionSourceText[] = "20 feet";
constexpr char kConversionResultText[] = "6.667 yards";
constexpr char kConversionCategoryText[] = "Length";
constexpr double kConversionSourceAmount = 20;
constexpr char kSourceRuleUnitText[] = "Foot";
constexpr double kSourceRuleTermA = 0.3048;
constexpr char kDestRuleUnitText[] = "Yard";
constexpr double kDestRuleTermA = 0.9144;
constexpr char kAlternativeDestRuleUnitText[] = "Inch";
constexpr double kAlternativeDestRuleTermA = 0.0254;

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

// Simulate a valid QuickAnswer definition response.
std::unique_ptr<QuickAnswersSession> CreateQuickAnswerDefinitionResponse() {
  std::unique_ptr<quick_answers::QuickAnswer> quick_answer =
      std::make_unique<quick_answers::QuickAnswer>();
  quick_answer->result_type = ResultType::kDefinitionResult;
  quick_answer->title.push_back(
      std::make_unique<quick_answers::QuickAnswerText>(
          l10n_util::GetStringFUTF8(IDS_QUICK_ANSWERS_DEFINITION_TITLE_TEXT,
                                    kQueryText, kPhoneticsText)));
  quick_answer->first_answer_row.push_back(
      std::make_unique<quick_answers::QuickAnswerResultText>(kDefinitionText));

  std::unique_ptr<DefinitionResult> definition_result =
      std::make_unique<DefinitionResult>();
  definition_result->word = base::UTF16ToUTF8(kQueryText);
  definition_result->word_class = kWordClassText;

  PhoneticsInfo phonetics_info;
  phonetics_info.text = base::UTF16ToUTF8(kPhoneticsText);
  phonetics_info.phonetics_audio = GURL(kPhoneticsAudioUrlWithProtocol);
  phonetics_info.locale = "en";
  definition_result->phonetics_info = phonetics_info;

  Sense sense;
  sense.definition = kDefinitionText;
  sense.sample_sentence = kSampleSentenceText;
  std::vector<std::string> synonyms_list{kFirstSynonymText, kSecondSynonymText,
                                         kThirdSynonymText};
  sense.synonyms_list = synonyms_list;
  definition_result->sense = sense;

  std::vector<Sense> subsenses_list;
  Sense subsense;
  subsense.definition = kSubsenseDefinitionText;
  subsense.sample_sentence = kSubsenseSampleSentenceText;
  std::vector<std::string> subsense_synonyms_list{kSubsenseSynonymText};
  subsense.synonyms_list = subsense_synonyms_list;
  subsenses_list.push_back(subsense);
  definition_result->subsenses_list = subsenses_list;

  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      std::make_unique<QuickAnswersSession>();
  quick_answers_session->quick_answer = std::move(quick_answer);
  quick_answers_session->structured_result =
      std::make_unique<StructuredResult>();
  quick_answers_session->structured_result->definition_result =
      std::move(definition_result);

  return quick_answers_session;
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
  translation_result->text_to_translate = base::UTF16ToUTF8(kSourceText);
  translation_result->translated_text = base::UTF16ToUTF8(kTranslatedText);
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

// Simulate a valid QuickAnswer unit conversion response.
std::unique_ptr<QuickAnswersSession> CreateQuickAnswerUnitConversionResponse() {
  std::unique_ptr<quick_answers::QuickAnswer> quick_answer =
      std::make_unique<quick_answers::QuickAnswer>();
  quick_answer->result_type = ResultType::kUnitConversionResult;
  quick_answer->title.push_back(
      std::make_unique<quick_answers::QuickAnswerText>(
          l10n_util::GetStringFUTF8(
              IDS_QUICK_ANSWERS_UNIT_CONVERSION_RESULT_TEXT, kSourceValueText,
              kSourceUnitText)));
  quick_answer->first_answer_row.push_back(
      std::make_unique<quick_answers::QuickAnswerResultText>(
          kConversionResultText));

  std::unique_ptr<UnitConversionResult> unit_conversion_result =
      std::make_unique<UnitConversionResult>();
  unit_conversion_result->source_text = kConversionSourceText;
  unit_conversion_result->result_text = kConversionResultText;
  unit_conversion_result->category = kConversionCategoryText;
  unit_conversion_result->source_amount = kConversionSourceAmount;

  std::optional<ConversionRule> source_rule = ConversionRule::Create(
      kConversionCategoryText, kSourceRuleUnitText, kSourceRuleTermA,
      /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule = ConversionRule::Create(
      kConversionCategoryText, kDestRuleUnitText, kDestRuleTermA,
      /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());
  unit_conversion_result->source_to_dest_unit_conversion =
      unit_conversion.value();

  std::vector<UnitConversion> alternative_unit_conversions_list;
  std::optional<ConversionRule> alternative_dest_rule = ConversionRule::Create(
      kConversionCategoryText, kAlternativeDestRuleUnitText,
      kAlternativeDestRuleTermA, /*term_b=*/std::nullopt,
      /*term_c=*/std::nullopt);
  std::optional<UnitConversion> alternative_unit_conversion =
      UnitConversion::Create(source_rule.value(),
                             alternative_dest_rule.value());
  alternative_unit_conversions_list.push_back(
      alternative_unit_conversion.value());
  unit_conversion_result->alternative_unit_conversions_list =
      alternative_unit_conversions_list;

  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      std::make_unique<QuickAnswersSession>();
  quick_answers_session->quick_answer = std::move(quick_answer);
  quick_answers_session->structured_result =
      std::make_unique<StructuredResult>();
  quick_answers_session->structured_result->unit_conversion_result =
      std::move(unit_conversion_result);

  return quick_answers_session;
}

}  // namespace

class QuickAnswersBrowserTest : public QuickAnswersBrowserTestBase {
 protected:
  void SetQuickAnswersEnabled(bool enabled) {
    if (IsMagicBoostEnabled()) {
      // Approve HMRConsentStatus to bypass opt-in flow.
      chromeos::MagicBoostState::Get()->AsyncWriteConsentStatus(
          chromeos::HMRConsentStatus::kApproved);
      chromeos::MagicBoostState::Get()->AsyncWriteHMREnabled(true);
    } else {
      // This simulates a behavior where a user enables QuickAnswers from
      // Settings.
      chrome_test_utils::GetProfile(this)->GetPrefs()->SetBoolean(
          prefs::kQuickAnswersEnabled, enabled);
    }
  }

  std::unique_ptr<MockQuickAnswersClient> CreateMockQuickAnswersClient() {
    return std::make_unique<MockQuickAnswersClient>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        controller()->GetQuickAnswersDelegate());
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

    NotificationDisplayServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this))
        ->Display(NotificationHandler::Type::TRANSIENT, notification,
                  /*metadata=*/nullptr);
  }

  // Trigger the Quick Answers widget and wait until it appears.
  views::Widget* ShowQuickAnswersWidget() {
    views::NamedWidgetShownWaiter quick_answers_view_widget_waiter(
        views::test::AnyWidgetTestPasskey(),
        chromeos::ReadWriteCardsUiController::kWidgetName);

    ShowMenuParams params;
    params.selected_text = kTestQuery;
    params.x = kCursorXToOverlapWithANotification;
    params.y = kCursorYToOverlapWithANotification;
    ShowMenuAndWait(params);

    return quick_answers_view_widget_waiter.WaitIfNeededAndGet();
  }

  void FakeControllerTimeTick() {
    CHECK(fake_time_tick_.is_null()) << "Fake is already enabled.";

    fake_time_tick_ = base::TimeTicks::Now();

    static_cast<QuickAnswersControllerImpl*>(controller())
        ->OverrideTimeTickNowForTesting(base::BindRepeating(
            &QuickAnswersBrowserTest::FakeTimeTickNow, base::Unretained(this)));
  }

  void FastForwardBy(base::TimeDelta delta) {
    CHECK(!fake_time_tick_.is_null()) << "Fake is not enabled.";

    fake_time_tick_ += delta;
  }

  base::TimeTicks FakeTimeTickNow() { return fake_time_tick_; }

  UserConsentView* GetUserConsentView() {
    return static_cast<QuickAnswersControllerImpl*>(controller())
        ->quick_answers_ui_controller()
        ->user_consent_view();
  }

  QuickAnswersView* GetQuickAnswersView() {
    return static_cast<QuickAnswersControllerImpl*>(controller())
        ->quick_answers_ui_controller()
        ->quick_answers_view();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  base::TimeTicks fake_time_tick_;
};

IN_PROC_BROWSER_TEST_P(QuickAnswersBrowserTest,
                       QuickAnswersViewAboveNotification) {
  SetQuickAnswersEnabled(true);

  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();
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
  EXPECT_FALSE(
      gfx::IntersectRects(message_popup_widget->GetWindowBoundsInScreen(),
                          quick_answers_view_widget->GetWindowBoundsInScreen())
          .IsEmpty());

  // TODO(b/239716419): Quick answers UI should be above the notification.
  EXPECT_TRUE(message_popup_widget->IsStackedAbove(
      quick_answers_view_widget->GetNativeView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersBrowserTest,
                       UserConsentViewAboveNotification) {
  if (IsMagicBoostEnabled()) {
    GTEST_SKIP() << "This test only applies when Magic Boost is disabled.";
  }

  // User consent view is stored within the `ReadWriteCardsUiController`'s
  // widget.
  views::NamedWidgetShownWaiter user_consent_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(),
      chromeos::ReadWriteCardsUiController::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenuAndWait(params);

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
  EXPECT_FALSE(
      gfx::IntersectRects(message_popup_widget->GetWindowBoundsInScreen(),
                          user_consent_view_widget->GetWindowBoundsInScreen())
          .IsEmpty());

  // TODO(b/239716419): Quick answers UI should be above the notification.
  EXPECT_TRUE(message_popup_widget->IsStackedAbove(
      user_consent_view_widget->GetNativeView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersBrowserTest, UserConsentViewImpressionCap) {
  if (IsMagicBoostEnabled()) {
    GTEST_SKIP() << "This test only applies when Magic Boost is disabled.";
  }

  FakeControllerTimeTick();

  for (int i = 0; i < kConsentImpressionCap; ++i) {
    ShowQuickAnswersWidget();
    ASSERT_EQ(QuickAnswersVisibility::kUserConsentVisible,
              controller()->GetQuickAnswersVisibility());

    FastForwardBy(base::Seconds(kConsentImpressionMinimumDuration));

    ui::test::EventGenerator event_generator(
        ash::Shell::GetPrimaryRootWindow());
    event_generator.PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);

    base::RunLoop run_loop;
    chrome_test_utils::GetProfile(this)->GetPrefs()->CommitPendingWrite(
        run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_FALSE(chrome_test_utils::GetProfile(this)->GetPrefs()->GetBoolean(
        prefs::kQuickAnswersEnabled));
    EXPECT_EQ(chrome_test_utils::GetProfile(this)->GetPrefs()->GetInteger(
                  prefs::kQuickAnswersConsentStatus),
              i == kConsentImpressionCap - 1
                  ? quick_answers::prefs::ConsentStatus::kRejected
                  : quick_answers::prefs::ConsentStatus::kUnknown)
        << "Consent status is set to kRejected once it reaches the impression "
           "cap.";
  }
}

IN_PROC_BROWSER_TEST_P(QuickAnswersBrowserTest, ClickAllowOnUserConsentView) {
  if (IsMagicBoostEnabled()) {
    GTEST_SKIP() << "This test only applies when Magic Boost is disabled.";
  }

  // User consent view is stored within the `ReadWriteCardsUiController`'s
  // widget.
  views::NamedWidgetShownWaiter user_consent_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(),
      chromeos::ReadWriteCardsUiController::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenuAndWait(params);

  views::Widget* user_consent_view_widget =
      user_consent_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(user_consent_view_widget);
  ASSERT_EQ(QuickAnswersVisibility::kUserConsentVisible,
            controller()->GetQuickAnswersVisibility());

  ASSERT_FALSE(chrome_test_utils::GetProfile(this)->GetPrefs()->GetBoolean(
      prefs::kQuickAnswersEnabled));

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(GetUserConsentView()
                            ->allow_button_for_test()
                            ->GetBoundsInScreen()
                            .CenterPoint());
  generator.ClickLeftButton();

  // Prefs should be set and quick answers view should be shown.
  EXPECT_TRUE(chrome_test_utils::GetProfile(this)->GetPrefs()->GetBoolean(
      prefs::kQuickAnswersEnabled));
  EXPECT_EQ(QuickAnswersVisibility::kQuickAnswersVisible,
            controller()->GetQuickAnswersVisibility());
}

IN_PROC_BROWSER_TEST_P(QuickAnswersBrowserTest,
                       ClickNoThanksOnUserConsentView) {
  if (IsMagicBoostEnabled()) {
    GTEST_SKIP() << "This test only applies when Magic Boost is disabled.";
  }

  // User consent view is stored within the `ReadWriteCardsUiController`'s
  // widget.
  views::NamedWidgetShownWaiter user_consent_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(),
      chromeos::ReadWriteCardsUiController::kWidgetName);

  ShowMenuParams params;
  params.selected_text = kTestQuery;
  params.x = kCursorXToOverlapWithANotification;
  params.y = kCursorYToOverlapWithANotification;
  ShowMenuAndWait(params);

  views::Widget* user_consent_view_widget =
      user_consent_view_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(user_consent_view_widget);
  ASSERT_EQ(QuickAnswersVisibility::kUserConsentVisible,
            controller()->GetQuickAnswersVisibility());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(GetUserConsentView()
                            ->no_thanks_button_for_test()
                            ->GetBoundsInScreen()
                            .CenterPoint());
  generator.ClickLeftButton();

  // Prefs should not be set and no views should be shown.
  EXPECT_FALSE(chrome_test_utils::GetProfile(this)->GetPrefs()->GetBoolean(
      prefs::kQuickAnswersEnabled));
  EXPECT_EQ(QuickAnswersVisibility::kClosed,
            controller()->GetQuickAnswersVisibility());
}

IN_PROC_BROWSER_TEST_P(QuickAnswersBrowserTest, SpokenFeedback) {
  constexpr char kSeparator[] = "; ";

  std::string expected_result_a11y_text =
      IsMagicBoostEnabled()
          ? base::JoinString({base::ReplaceStringPlaceholders(
                                  "$1 · slash $2 slash ",
                                  {base::UTF16ToUTF8(kQueryText),
                                   base::UTF16ToUTF8(kPhoneticsText)},
                                  nullptr),
                              kDefinitionText},
                             kSeparator)
          : base::JoinString({"Define",
                              base::ReplaceStringPlaceholders(
                                  "$1 · slash $2 slash ",
                                  {base::UTF16ToUTF8(kQueryText),
                                   base::UTF16ToUTF8(kPhoneticsText)},
                                  nullptr),
                              kDefinitionText},
                             kSeparator);

  SetQuickAnswersEnabled(true);

  std::unique_ptr<MockQuickAnswersClient> mock_quick_answers_client =
      CreateMockQuickAnswersClient();
  base::test::TestFuture<void> on_quick_answers_click_future;
  EXPECT_CALL(*(mock_quick_answers_client.get()), OnQuickAnswerClick)
      .WillOnce(base::test::InvokeFuture(on_quick_answers_click_future));
  controller()->SetClient(std::move(mock_quick_answers_client));

  ash::test::SpeechMonitor speech_monitor;
  speech_monitor.Call(
      []() { ash::AccessibilityManager::Get()->EnableSpokenFeedback(true); });
  speech_monitor.ExpectSpeech("ChromeVox spoken feedback is ready");
  speech_monitor.Call([this]() {
    ShowMenuParams params;
    params.selected_text = kTestQuery;
    params.x = kCursorXToOverlapWithANotification;
    params.y = kCursorYToOverlapWithANotification;

    // Use `ShowMenu` instead of `ShowMenuAndWait`. `ShowMenuAndWait` creates a
    // `RunLoop`. `SpeechMonitor` might already have a message loop. Nested
    // `RunLoop` is not enabled by default. We wait menu shown with menu opened
    // spoken feedback.
    ShowMenu(params);
  });
  speech_monitor.ExpectSpeech("menu opened");
  speech_monitor.Call([this]() {
    controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
        CreateQuickAnswerDefinitionResponse());
  });
  speech_monitor.ExpectSpeech(
      "Info related to your selection available. Use Up arrow key to access.");
  speech_monitor.Call([]() {
    ui::test::EventGenerator event_generator(
        ash::Shell::GetPrimaryRootWindow());
    ui::test::EmulateFullKeyPressReleaseSequence(
        &event_generator, ui::KeyboardCode::VKEY_UP, /*control=*/false,
        /*shift=*/false, /*alt=*/false, /*command=*/false);
  });
  speech_monitor.ExpectSpeech("Info related to your selection");
  speech_monitor.ExpectSpeech("Dialog");
  speech_monitor.ExpectSpeech(expected_result_a11y_text);
  speech_monitor.ExpectSpeech("Press Search plus Space to activate");
  speech_monitor.Call([]() {
    ui::test::EventGenerator event_generator(
        ash::Shell::GetPrimaryRootWindow());
    ui::test::EmulateFullKeyPressReleaseSequence(
        &event_generator, ui::KeyboardCode::VKEY_SPACE, /*control=*/false,
        /*shift=*/false, /*alt=*/false, /*command=*/true);
  });
  speech_monitor.Replay();

  // Wait after `SpeechMonitor::Replay`. `Replay` can create a message loop. If
  // we wait inside reply, it can create a nested `RunLoop`, which is not
  // enabled by default.
  EXPECT_TRUE(on_quick_answers_click_future.Wait())
      << "Expected OnQuickAnswersClick. But it is not called";
  EXPECT_EQ(controller()->GetQuickAnswersVisibility(),
            QuickAnswersVisibility::kClosed);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    QuickAnswersBrowserTest,
    ::testing::Bool());

class RichAnswersBrowserTest : public QuickAnswersBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    QuickAnswersBrowserTestBase::SetUpOnMainThread();
    SetQuickAnswersEnabled(true);

    event_generator_.emplace(ash::Shell::GetPrimaryRootWindow());
  }

  // Trigger the Rich Answers widget by clicking on |quick_answers_view_widget|,
  // and wait until it appears.
  views::Widget* ShowRichAnswersWidget(
      views::Widget* quick_answers_view_widget) {
    views::NamedWidgetShownWaiter rich_answers_view_widget_waiter(
        views::test::AnyWidgetTestPasskey(), RichAnswersView::kWidgetName);
    event_generator_->MoveMouseTo(
        quick_answers_view_widget->GetWindowBoundsInScreen().CenterPoint());
    event_generator_->ClickLeftButton();

    views::Widget* rich_answers_view_widget =
        rich_answers_view_widget_waiter.WaitIfNeededAndGet();

    return rich_answers_view_widget;
  }

  std::optional<ui::test::EventGenerator> event_generator_;

 private:
  base::test::ScopedFeatureList feature_list_{
      chromeos::features::kQuickAnswersRichCard};
};

IN_PROC_BROWSER_TEST_P(RichAnswersBrowserTest,
                       RichAnswersNotTriggeredOnInvalidResult) {
  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();

  // Click on the Quick Answers widget. This should *not* trigger the
  // Rich Answers widget since no valid QuickAnswer result is provided.
  views::NamedWidgetShownWaiter rich_answers_view_widget_waiter(
      views::test::AnyWidgetTestPasskey(), RichAnswersView::kWidgetName);
  event_generator_->MoveMouseTo(
      quick_answers_view_widget->GetWindowBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  // Check that all quick answers views are closed.
  EXPECT_TRUE(quick_answers_view_widget->IsClosed());
  EXPECT_TRUE(controller()->GetQuickAnswersVisibility() ==
              QuickAnswersVisibility::kClosed);
}

IN_PROC_BROWSER_TEST_P(RichAnswersBrowserTest,
                       RichAnswersTriggeredAndDismissedOnValidResult) {
  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();

  // Simulate having received a valid QuickAnswer response.
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerTranslationResponse());

  views::Widget* rich_answers_view_widget =
      ShowRichAnswersWidget(quick_answers_view_widget);
  ASSERT_TRUE(rich_answers_view_widget != nullptr);

  // Check that the quick answers view closes when the rich answers view shows.
  EXPECT_TRUE(quick_answers_view_widget->IsClosed());
  EXPECT_TRUE(controller()->GetQuickAnswersVisibility() ==
              QuickAnswersVisibility::kRichAnswersVisible);

  // Click outside the rich answers view window bounds to dismiss it.
  gfx::Rect rich_answers_bounds =
      rich_answers_view_widget->GetWindowBoundsInScreen();
  event_generator_->MoveMouseTo(
      gfx::Point(rich_answers_bounds.x() / 2, rich_answers_bounds.y() / 2));
  event_generator_->ClickLeftButton();

  // Check that the rich answers view is dismissed.
  EXPECT_TRUE(rich_answers_view_widget->IsClosed());
  EXPECT_TRUE(controller()->GetQuickAnswersVisibility() ==
              QuickAnswersVisibility::kClosed);
}

IN_PROC_BROWSER_TEST_P(RichAnswersBrowserTest,
                       DefinitionResultCardContentsCorrectlyShown) {
  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();

  // Simulate having received a valid QuickAnswer definition response.
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerDefinitionResponse());

  // Check that the shown result type icon on the QuickAnswersView
  // correctly corresponds to the definition result type.
  ui::ImageModel expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerDictionaryIcon, ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kQuickAnswersResultTypeIconSizeDip);

  views::Widget* rich_answers_view_widget =
      ShowRichAnswersWidget(quick_answers_view_widget);

  // Check that the shown result type icon on the RichAnswersView
  // correctly corresponds to the definition result type.
  RichAnswersView* rich_answers_view = static_cast<RichAnswersView*>(
      rich_answers_view_widget->GetContentsView());
  expected_image_model = ui::ImageModel::FromVectorIcon(
      chromeos::kDictionaryIcon, ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kRichAnswersResultTypeIconSizeDip);
  EXPECT_TRUE(rich_answers_view->GetIconImageModelForTesting() ==
              expected_image_model);

  // TODO(b/326370198): Add checks for other card contents.
}

IN_PROC_BROWSER_TEST_P(RichAnswersBrowserTest,
                       TranslationResultCardContentsCorrectlyShown) {
  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();

  // Simulate having received a valid QuickAnswer translation response.
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerTranslationResponse());

  // Check that the shown result type icon on the QuickAnswersView
  // correctly corresponds to the translation result type.
  ui::ImageModel expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerTranslationIcon, ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kQuickAnswersResultTypeIconSizeDip);

  views::Widget* rich_answers_view_widget =
      ShowRichAnswersWidget(quick_answers_view_widget);

  // Check that the shown result type icon on the RichAnswersView
  // correctly corresponds to the translation result type.
  RichAnswersView* rich_answers_view = static_cast<RichAnswersView*>(
      rich_answers_view_widget->GetContentsView());
  expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerTranslationIcon, ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kRichAnswersResultTypeIconSizeDip);
  EXPECT_TRUE(rich_answers_view->GetIconImageModelForTesting() ==
              expected_image_model);

  // TODO(b/326370198): Add checks for other card contents.
}

IN_PROC_BROWSER_TEST_P(RichAnswersBrowserTest,
                       UnitConversionResultCardContentsCorrectlyShown) {
  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();

  // Simulate having received a valid QuickAnswer unit conversion response.
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerUnitConversionResponse());

  // Check that the shown result type icon on the QuickAnswersView
  // correctly corresponds to the unit conversion result type.
  ui::ImageModel expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerCalculatorIcon, ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kQuickAnswersResultTypeIconSizeDip);

  views::Widget* rich_answers_view_widget =
      ShowRichAnswersWidget(quick_answers_view_widget);

  // Check that the shown result type icon on the RichAnswersView
  // correctly corresponds to the unit conversion result type.
  RichAnswersView* rich_answers_view = static_cast<RichAnswersView*>(
      rich_answers_view_widget->GetContentsView());
  expected_image_model = ui::ImageModel::FromVectorIcon(
      omnibox::kAnswerCalculatorIcon, ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kRichAnswersResultTypeIconSizeDip);
  EXPECT_TRUE(rich_answers_view->GetIconImageModelForTesting() ==
              expected_image_model);

  // TODO(b/326370198): Add checks for other card contents.
}

IN_PROC_BROWSER_TEST_P(RichAnswersBrowserTest, AccessibleProperties) {
  views::Widget* quick_answers_view_widget = ShowQuickAnswersWidget();
  controller()->GetQuickAnswersDelegate()->OnQuickAnswerReceived(
      CreateQuickAnswerUnitConversionResponse());
  RichAnswersView* rich_answers_view = static_cast<RichAnswersView*>(
      ShowRichAnswersWidget(quick_answers_view_widget)->GetContentsView());
  ui::AXNodeData data;

  ASSERT_TRUE(rich_answers_view);
  rich_answers_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF8(IDS_RICH_ANSWERS_VIEW_A11Y_NAME_TEXT));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    RichAnswersBrowserTest,
    ::testing::Bool());

}  // namespace quick_answers

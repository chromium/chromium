// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_message.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/messages/android/message_enums.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using testing::MockTranslateClient;
using testing::MockTranslateRanker;
using ::testing::Return;
using ::testing::Test;
using ::testing::Truly;

class TestBridge : public TranslateMessage::Bridge {
 public:
  ~TestBridge() override;

  base::WeakPtr<TestBridge> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool,
              CreateTranslateMessage,
              (JNIEnv*, content::WebContents*, TranslateMessage*, jint),
              (override));

  MOCK_METHOD(void,
              ShowTranslateError,
              (JNIEnv*, content::WebContents*),
              (override));

  MOCK_METHOD(void,
              ShowMessage,
              (JNIEnv*,
               base::android::ScopedJavaLocalRef<jstring>,
               base::android::ScopedJavaLocalRef<jstring>,
               base::android::ScopedJavaLocalRef<jstring>,
               jboolean),
              (override));

  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobjectArray>,
              ConstructMenuItemArray,
              (JNIEnv*,
               base::android::ScopedJavaLocalRef<jobjectArray>,
               base::android::ScopedJavaLocalRef<jobjectArray>,
               base::android::ScopedJavaLocalRef<jbooleanArray>,
               base::android::ScopedJavaLocalRef<jintArray>,
               base::android::ScopedJavaLocalRef<jobjectArray>),
              (override));

  MOCK_METHOD(void, ClearNativePointer, (JNIEnv*), (override));
  MOCK_METHOD(void, Dismiss, (JNIEnv*), (override));

 private:
  base::WeakPtrFactory<TestBridge> weak_ptr_factory_{this};
};

TestBridge::~TestBridge() = default;

class TestLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class TestTranslateDriver : public testing::MockTranslateDriver {
 public:
  ~TestTranslateDriver() override;

  MOCK_METHOD(void, RevertTranslation, (int), (override));
  MOCK_METHOD(bool, IsIncognito, (), (const override));
  MOCK_METHOD(bool, HasCurrentPage, (), (const override));

  const GURL& GetLastCommittedURL() const override { return url_; }
  void SetLastCommittedURL(GURL url) { url_ = std::move(url); }

 private:
  GURL url_;
};

TestTranslateDriver::~TestTranslateDriver() = default;

class JavaStringMatcher {
 public:
  JavaStringMatcher(JNIEnv* env, std::u16string expected)
      : env_(env), expected_(std::move(expected)) {}

  bool operator()(
      const base::android::ScopedJavaLocalRef<jstring>& actual) const {
    return base::android::ConvertJavaStringToUTF16(env_, actual) == expected_;
  }

 private:
  raw_ptr<JNIEnv> env_;
  std::u16string expected_;
};

bool IsJavaStringNonNull(
    const base::android::ScopedJavaLocalRef<jstring>& java_string) {
  return static_cast<bool>(java_string);
}
bool IsJavaStringNull(
    const base::android::ScopedJavaLocalRef<jstring>& java_string) {
  return !java_string;
}

base::android::ScopedJavaLocalRef<jobjectArray> CreateTestJobjectArray(
    JNIEnv* env) {
  std::string test_strings[] = {"test", "opaque", "jobjectArray"};
  return base::android::ToJavaArrayOfStrings(env, test_strings);
}

constexpr int kDefaultDismissalDurationSeconds = 10;

struct SecondaryMenuItem {
  TranslateMessage::OverflowMenuItemId id;
  bool has_checkmark;
  std::string language_code;
};

constexpr const char kInfobarEventHistogram[] =
    "Translate.CompactInfobar.Event";

class TranslateMessageTest : public ::testing::Test {
 public:
  TranslateMessageTest() {
    ON_CALL(driver_, IsIncognito()).WillByDefault(Return(false));
    ON_CALL(driver_, HasCurrentPage()).WillByDefault(Return(true));
    driver_.SetLastCommittedURL(GURL("http://www.example.com/"));
  }

  void ShowBeforeTranslationMessage(JNIEnv* env,
                                    const std::string& source_language_code,
                                    const std::string& target_language_code) {
    EXPECT_CALL(*bridge_,
                ShowMessage(env,
                            /*title=*/Truly(IsJavaStringNonNull),
                            /*description=*/Truly(IsJavaStringNonNull),
                            /*primary_button_text=*/Truly(IsJavaStringNonNull),
                            /*has_overflow_menu=*/true));

    translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE,
                                          source_language_code,
                                          target_language_code);
  }

  void ExpectTranslationInProgress(JNIEnv* env,
                                   const std::string& source_language_code,
                                   const std::string& target_language_code) {
    EXPECT_CALL(*bridge_,
                ShowMessage(env,
                            /*title=*/Truly(IsJavaStringNonNull),
                            /*description=*/Truly(IsJavaStringNonNull),
                            /*primary_button_text=*/Truly(IsJavaStringNull),
                            /*has_overflow_menu=*/true));

    EXPECT_CALL(
        *client_,
        ShowTranslateUI(TRANSLATE_STEP_TRANSLATING, source_language_code,
                        target_language_code, TranslateErrors::NONE, false))
        .WillOnce(DoAll(InvokeWithoutArgs([this, source_language_code,
                                           target_language_code]() {
                          translate_message_->ShowTranslateStep(
                              TRANSLATE_STEP_TRANSLATING, source_language_code,
                              target_language_code);
                        }),
                        Return(true)));
  }

  void FinishTranslation(JNIEnv* env,
                         const std::string& source_language_code,
                         const std::string& target_language_code) {
    EXPECT_CALL(*bridge_,
                ShowMessage(env,
                            /*title=*/Truly(IsJavaStringNonNull),
                            /*description=*/Truly(IsJavaStringNonNull),
                            /*primary_button_text=*/Truly(IsJavaStringNonNull),
                            /*has_overflow_menu=*/true));

    EXPECT_CALL(
        *client_,
        ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, source_language_code,
                        target_language_code, TranslateErrors::NONE, false))
        .WillOnce(DoAll(InvokeWithoutArgs([this, source_language_code,
                                           target_language_code]() {
                          translate_message_->ShowTranslateStep(
                              TRANSLATE_STEP_AFTER_TRANSLATE,
                              source_language_code, target_language_code);
                        }),
                        Return(true)));

    manager_->PageTranslated(source_language_code, target_language_code,
                             TranslateErrors::NONE);
  }

  void ExpectTranslationReverts(JNIEnv* env,
                                const std::string& source_language_code,
                                const std::string& target_language_code) {
    EXPECT_CALL(driver_, RevertTranslation(_));

    EXPECT_CALL(*bridge_,
                ShowMessage(env,
                            /*title=*/Truly(IsJavaStringNonNull),
                            /*description=*/Truly(IsJavaStringNonNull),
                            /*primary_button_text=*/Truly(IsJavaStringNonNull),
                            /*has_overflow_menu=*/true));
  }

  void TranslateThenRevertThenDismiss(JNIEnv* env,
                                      const std::string& source_language_code,
                                      const std::string& target_language_code) {
    ShowBeforeTranslationMessage(env, source_language_code,
                                 target_language_code);

    ExpectTranslationInProgress(env, source_language_code,
                                target_language_code);
    translate_message_->HandlePrimaryAction(env);

    FinishTranslation(env, source_language_code, target_language_code);

    ExpectTranslationReverts(env, source_language_code, target_language_code);
    translate_message_->HandlePrimaryAction(env);

    // Simulate a dismissal triggered from the Java side.
    EXPECT_CALL(*bridge_, ClearNativePointer(env));
    int prev_on_dismiss_callback_called_count =
        on_dismiss_callback_called_count_;
    translate_message_->HandleDismiss(
        env, static_cast<jint>(messages::DismissReason::TIMER));

    // The on-dismiss callback should have been called.
    EXPECT_EQ(prev_on_dismiss_callback_called_count + 1,
              on_dismiss_callback_called_count_);
  }

  void ExpectConstructMenuItemArray(
      JNIEnv* env,
      std::vector<SecondaryMenuItem> expected_items,
      base::android::ScopedJavaLocalRef<jobjectArray> return_value = nullptr) {
    EXPECT_CALL(
        *bridge_,
        ConstructMenuItemArray(
            env,
            /*titles=*/_,
            /*subtitles=*/_,
            /*has_checkmarks=*/
            Truly([env, expected_items](
                      const base::android::ScopedJavaLocalRef<jbooleanArray>&
                          actual) {
              std::vector<bool> actual_vector;
              base::android::JavaBooleanArrayToBoolVector(env, actual,
                                                          &actual_vector);
              return base::ranges::equal(expected_items, actual_vector,
                                         std::equal_to<>(),
                                         &SecondaryMenuItem::has_checkmark);
            }),
            /*overflow_menu_item_ids=*/
            Truly([env, expected_items](
                      const base::android::ScopedJavaLocalRef<jintArray>&
                          actual) {
              std::vector<int> actual_vector;
              base::android::JavaIntArrayToIntVector(env, actual,
                                                     &actual_vector);
              return base::ranges::equal(expected_items, actual_vector,
                                         std::equal_to<>(),
                                         [](const SecondaryMenuItem& item) {
                                           return static_cast<int>(item.id);
                                         });
            }),
            /*language_codes=*/
            Truly([env, expected_items](
                      const base::android::ScopedJavaLocalRef<jobjectArray>&
                          actual) {
              std::vector<std::string> actual_vector;
              base::android::AppendJavaStringArrayToStringVector(
                  env, actual, &actual_vector);
              return base::ranges::equal(expected_items, actual_vector,
                                         std::equal_to<>(),
                                         &SecondaryMenuItem::language_code);
            })))
        .WillOnce(Return(return_value));
  }

 protected:
  void SetUp() override {
    ::testing::Test::SetUp();

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->SetString(testing::accept_languages_prefs, std::string());
    pref_service_->SetString(language::prefs::kAcceptLanguages, std::string());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
    ranker_ = std::make_unique<MockTranslateRanker>();
    client_ =
        std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
    manager_ = std::make_unique<TranslateManager>(client_.get(), ranker_.get(),
                                                  language_model_.get());
    manager_->GetLanguageState()->set_translation_declined(false);
    TranslateDownloadManager::GetInstance()->set_application_locale("en");
    translate_prefs_ = client_->GetTranslatePrefs();

    auto owned_bridge = std::make_unique<TestBridge>();
    bridge_ = owned_bridge->GetWeakPtr();

    translate_message_ = std::make_unique<TranslateMessage>(
        /*web_contents=*/nullptr, manager_->GetWeakPtr(),
        base::BindRepeating(
            [](int* on_dismiss_callback_called_count) {
              ++(*on_dismiss_callback_called_count);
            },
            &on_dismiss_callback_called_count_),
        std::move(owned_bridge));
  }

  TestTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<TestLanguageModel> language_model_;
  std::unique_ptr<TranslateManager> manager_;
  std::unique_ptr<MockTranslateRanker> ranker_;
  std::unique_ptr<TranslatePrefs> translate_prefs_;

  base::WeakPtr<TestBridge> bridge_;
  std::unique_ptr<TranslateMessage> translate_message_;
  int on_dismiss_callback_called_count_ = 0;
};

TEST_F(TranslateMessageTest, TranslateAndRevert) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));

  {
    base::HistogramTester histogram_tester;
    ShowBeforeTranslationMessage(env, "fr", "en");
    histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                        InfobarEvent::INFOBAR_IMPRESSION, 1);
  }

  {
    base::HistogramTester histogram_tester;
    ExpectTranslationInProgress(env, "fr", "en");
    translate_message_->HandlePrimaryAction(env);
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_TARGET_TAB_TRANSLATE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    FinishTranslation(env, "fr", "en");
    histogram_tester.ExpectTotalCount(kInfobarEventHistogram, 0);
  }

  {
    base::HistogramTester histogram_tester;
    ExpectTranslationReverts(env, "fr", "en");
    translate_message_->HandlePrimaryAction(env);
    histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                        InfobarEvent::INFOBAR_REVERT, 1);
  }

  // Simulate a dismissal triggered by the Java side.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);
}

TEST_F(TranslateMessageTest, TranslateAndRevertMultipleTimes) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  TranslateThenRevertThenDismiss(env, "fr", "en");

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  TranslateThenRevertThenDismiss(env, "de", "es");
}

TEST_F(TranslateMessageTest, ShowErrorBeforeTranslation) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("fr");
  manager_->GetLanguageState()->SetCurrentLanguage("fr");

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(*bridge_,
              ShowMessage(env,
                          /*title=*/Truly(IsJavaStringNonNull),
                          /*description=*/Truly(IsJavaStringNonNull),
                          /*primary_button_text=*/Truly(IsJavaStringNonNull),
                          /*has_overflow_menu=*/true));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "fr",
                                        "en");
}

TEST_F(TranslateMessageTest, ShowErrorAfterTranslation) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("fr");
  manager_->GetLanguageState()->SetCurrentLanguage("en");

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(*bridge_,
              ShowMessage(env,
                          /*title=*/Truly(IsJavaStringNonNull),
                          /*description=*/Truly(IsJavaStringNonNull),
                          /*primary_button_text=*/Truly(IsJavaStringNonNull),
                          /*has_overflow_menu=*/true));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "fr",
                                        "en");
}

TEST_F(TranslateMessageTest, DismissMessageOnDestruction) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));

  ShowBeforeTranslationMessage(env, "fr", "en");

  EXPECT_CALL(*bridge_, Dismiss(env))
      .WillOnce(InvokeWithoutArgs([env, message = translate_message_.get()]() {
        message->HandleDismiss(
            env,
            static_cast<jint>(messages::DismissReason::DISMISSED_BY_FEATURE));
      }));
  EXPECT_CALL(*bridge_, ClearNativePointer(env));

  translate_message_.reset();

  // By design, the on_dismiss_callback_ will not be called, since it's cleared
  // in the TranslateMessage destructor before dismissing the message, in order
  // to prevent a use-after-free bug.
  EXPECT_EQ(0, on_dismiss_callback_called_count_);
}

TEST_F(TranslateMessageTest, ShowOverflowMenu) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));

  ShowBeforeTranslationMessage(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  base::HistogramTester histogram_tester;
  translate_message_->BuildOverflowMenu(env);
  histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                      InfobarEvent::INFOBAR_OPTIONS, 1);
}

TEST_F(TranslateMessageTest, OverflowMenuToggleAlwaysTranslateLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));

  ShowBeforeTranslationMessage(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  {
    base::HistogramTester histogram_tester;
    // Toggle "Always translate pages in <language>" to on.
    ExpectTranslationInProgress(env, "fr", "en");
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(TranslateMessage::OverflowMenuItemId::
                             kToggleAlwaysTranslateLanguage),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(false)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_ALWAYS_TRANSLATE, 1);
  }

  EXPECT_TRUE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  FinishTranslation(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            true, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);

  {
    base::HistogramTester histogram_tester;
    // Toggle "Always translate pages in <language>" to off.
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(TranslateMessage::OverflowMenuItemId::
                             kToggleAlwaysTranslateLanguage),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(true)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_ALWAYS_TRANSLATE_UNDO, 1);
  }

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
}

TEST_F(TranslateMessageTest, OverflowMenuToggleNeverTranslateLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Begin from a translated page.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  FinishTranslation(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);
  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));

  {
    base::HistogramTester histogram_tester;
    // Toggle "Never translate pages in <language>" to on.
    ExpectTranslationReverts(env, "fr", "en");
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(TranslateMessage::OverflowMenuItemId::
                             kToggleNeverTranslateLanguage),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(false)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_NEVER_TRANSLATE, 1);
  }

  EXPECT_TRUE(translate_prefs_->IsBlockedLanguage("fr"));

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            true, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);

  {
    base::HistogramTester histogram_tester;
    // Toggle "Never translate pages in <language>" to off.
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(TranslateMessage::OverflowMenuItemId::
                             kToggleNeverTranslateLanguage),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(true)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_NEVER_TRANSLATE_UNDO, 1);
  }

  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));
}

TEST_F(TranslateMessageTest, OverflowMenuToggleNeverTranslateSite) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Begin from a translated page.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  FinishTranslation(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);
  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList(
      driver_.GetLastCommittedURL().HostNoBracketsPiece()));

  {
    base::HistogramTester histogram_tester;
    // Toggle "Never translate this site" to on.
    ExpectTranslationReverts(env, "fr", "en");
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(
            TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(false)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE, 1);
  }

  EXPECT_TRUE(translate_prefs_->IsSiteOnNeverPromptList(
      driver_.GetLastCommittedURL().HostNoBracketsPiece()));

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            true, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);

  {
    base::HistogramTester histogram_tester;
    // Toggle "Never translate this site" to off.
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(
            TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(true)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE_UNDO,
        1);
  }

  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList(
      driver_.GetLastCommittedURL().HostNoBracketsPiece()));
}

TEST_F(TranslateMessageTest, OverflowMenuChangeSourceLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  translate_prefs_->AddToLanguageList("en", true);
  translate_prefs_->AddToLanguageList("es", true);
  translate_prefs_->AddToLanguageList("de", true);

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}),
      CreateTestJobjectArray(env));

  EXPECT_TRUE(translate_message_->BuildOverflowMenu(env));

  std::vector<SecondaryMenuItem> menu_items;
  TranslateUIDelegate ui_delegate(manager_->GetWeakPtr(), "fr", "en");
  for (size_t i = 0U;
       i < ui_delegate.translate_ui_languages_manager()->GetNumberOfLanguages();
       ++i) {
    std::string language_code =
        ui_delegate.translate_ui_languages_manager()->GetLanguageCodeAt(i);
    if (language_code == "fr")
      continue;
    menu_items.emplace_back(SecondaryMenuItem{
        TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
        std::move(language_code)});
  }

  {
    base::HistogramTester histogram_tester;
    // Click the kChangeSourceLanguage option in the overflow menu, which should
    // return a list of language picker menu items.
    ExpectConstructMenuItemArray(env, menu_items, CreateTestJobjectArray(env));
    EXPECT_TRUE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(
            TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(false)));
    histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                        InfobarEvent::INFOBAR_PAGE_NOT_IN, 1);
  }

  // Clicking a language should kick off a translation.
  ExpectTranslationInProgress(env, "de", "en");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage),
      base::android::ConvertUTF8ToJavaString(env, "de"),
      static_cast<jboolean>(false)));

  FinishTranslation(env, "de", "en");
}

TEST_F(TranslateMessageTest,
       OverflowMenuChangeTargetLanguageNoContentLanguages) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      language::kContentLanguagesInLanguagePicker);

  translate_prefs_->AddToLanguageList("en", true);
  translate_prefs_->AddToLanguageList("es", true);
  translate_prefs_->AddToLanguageList("de", true);

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}),
      CreateTestJobjectArray(env));

  EXPECT_TRUE(translate_message_->BuildOverflowMenu(env));

  std::vector<SecondaryMenuItem> menu_items;
  TranslateUIDelegate ui_delegate(manager_->GetWeakPtr(), "fr", "en");
  for (size_t i = 0U;
       i < ui_delegate.translate_ui_languages_manager()->GetNumberOfLanguages();
       ++i) {
    std::string language_code =
        ui_delegate.translate_ui_languages_manager()->GetLanguageCodeAt(i);
    if (language_code == "en" || language_code == kUnknownLanguageCode)
      continue;
    menu_items.emplace_back(SecondaryMenuItem{
        TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
        std::move(language_code)});
  }

  {
    base::HistogramTester histogram_tester;
    // Click the kChangeTargetLanguage option in the overflow menu, which should
    // return a list of language picker menu items.
    ExpectConstructMenuItemArray(env, menu_items, CreateTestJobjectArray(env));
    EXPECT_TRUE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(
            TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage),
        base::android::ConvertUTF8ToJavaString(env, std::string()),
        static_cast<jboolean>(false)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_MORE_LANGUAGES, 1);
  }

  {
    base::HistogramTester histogram_tester;
    // Clicking a language should kick off a translation.
    ExpectTranslationInProgress(env, "fr", "de");
    EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
        env,
        static_cast<int>(
            TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage),
        base::android::ConvertUTF8ToJavaString(env, "de"),
        static_cast<jboolean>(false)));
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram, InfobarEvent::INFOBAR_MORE_LANGUAGES_TRANSLATE,
        1);
  }

  FinishTranslation(env, "fr", "de");
}

TEST_F(TranslateMessageTest,
       OverflowMenuChangeTargetLanguageWithContentLanguages) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      language::kContentLanguagesInLanguagePicker);

  translate_prefs_->AddToLanguageList("en", true);
  translate_prefs_->AddToLanguageList("es", true);
  translate_prefs_->AddToLanguageList("de", true);

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}),
      CreateTestJobjectArray(env));

  EXPECT_TRUE(translate_message_->BuildOverflowMenu(env));

  std::vector<SecondaryMenuItem> menu_items = {
      {TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
       "es"},
      {TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
       "de"},
      // Divider after content languages.
      {TranslateMessage::OverflowMenuItemId::kInvalid, false, std::string()},
  };

  TranslateUIDelegate ui_delegate(manager_->GetWeakPtr(), "fr", "en");
  for (size_t i = 0U;
       i < ui_delegate.translate_ui_languages_manager()->GetNumberOfLanguages();
       ++i) {
    std::string language_code =
        ui_delegate.translate_ui_languages_manager()->GetLanguageCodeAt(i);
    if (language_code == "en" || language_code == kUnknownLanguageCode)
      continue;
    menu_items.emplace_back(SecondaryMenuItem{
        TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
        std::move(language_code)});
  }

  // Click the kChangeTargetLanguage option in the overflow menu, which should
  // return a list of language picker menu items.
  ExpectConstructMenuItemArray(env, menu_items, CreateTestJobjectArray(env));
  EXPECT_TRUE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(false)));

  // Clicking a language should kick off a translation.
  ExpectTranslationInProgress(env, "fr", "de");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage),
      base::android::ConvertUTF8ToJavaString(env, "de"),
      static_cast<jboolean>(false)));

  FinishTranslation(env, "fr", "de");
}

TEST_F(TranslateMessageTest, OverflowMenuIncognito) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ON_CALL(driver_, IsIncognito()).WillByDefault(Return(true));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  // Doesn't include the kToggleAlwaysTranslateLanguage option.
  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);
}

TEST_F(TranslateMessageTest, OverflowMenuEmptyUrl) {
  JNIEnv* env = base::android::AttachCurrentThread();
  driver_.SetLastCommittedURL(GURL());

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  // Doesn't include the kToggleNeverTranslateSite option.
  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::
                kToggleAlwaysTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);
}

TEST_F(TranslateMessageTest, OverflowMenuUnknownSourceLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, kUnknownLanguageCode, "en");

  // Doesn't include the kToggleAlwaysTranslateLanguage option or the
  // kToggleNeverTranslateLanguage option.
  ExpectConstructMenuItemArray(
      env,
      std::vector<SecondaryMenuItem>(
          {{TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kInvalid, false,
            std::string()},
           {TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite,
            false, std::string()},
           {TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
            std::string()}}));

  translate_message_->BuildOverflowMenu(env);
}

TEST_F(TranslateMessageTest, CreateTranslateMessageFails) {
  JNIEnv* env = base::android::AttachCurrentThread();

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(false));

  // ShowMessage should not be called after CreateTranslateMessage fails.
  EXPECT_CALL(*bridge_, ShowMessage(_, _, _, _, _)).Times(0);

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE, "fr",
                                        "en");
}

TEST_F(TranslateMessageTest, CreateTranslateMessageFailsThenSucceeds) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // The first call to CreateTranslateMessage will fail.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(false));

  // ShowMessage should not be called after CreateTranslateMessage fails.
  EXPECT_CALL(*bridge_, ShowMessage(_, _, _, _, _)).Times(0);

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE, "fr",
                                        "en");

  // The second call to CreateTranslateMessage will succeed, and test that the
  // whole process of translating, reverting, and dismissing works properly
  // afterwards.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  TranslateThenRevertThenDismiss(env, "fr", "en");
}

TEST_F(TranslateMessageTest, CreateTranslateMessageSucceedsThenFails) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // The first call to CreateTranslateMessage will succeed.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  TranslateThenRevertThenDismiss(env, "fr", "en");

  // The second call to CreateTranslateMessage will fail.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(false));

  // ShowMessage should not be called after CreateTranslateMessage fails.
  EXPECT_CALL(*bridge_, ShowMessage(_, _, _, _, _)).Times(0);
  translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE, "fr",
                                        "en");
}

TEST_F(TranslateMessageTest, TranslationDismissedInProgressByTimer) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateDeniedCount)
      ->Set("fr", 100);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateIgnoredCount)
      ->Set("fr", 100);

  // Show the translate message and click "Translate".
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);

  EXPECT_EQ(1, translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));

  // Dismiss the translate message while translation is still in-progress.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  EXPECT_EQ(1, translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationDismissedInProgressByGesture) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateDeniedCount)
      ->Set("fr", 100);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateIgnoredCount)
      ->Set("fr", 100);

  // Show the translate message and click "Translate".
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);

  EXPECT_EQ(1, translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));

  // Dismiss the translate message while translation is still in-progress.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  EXPECT_EQ(1, translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationIgnored) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", 100);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateDeniedCount)
      ->Set("fr", 100);

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));

  histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                      InfobarEvent::INFOBAR_DECLINE, 1);

  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  EXPECT_EQ(100, translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(100, translate_prefs_->GetTranslationDeniedCount("fr"));

  EXPECT_EQ(1, translate_prefs_->GetTranslationIgnoredCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationNotIgnoredBecauseOverflowMenuOpened) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Show the translate message and simulate the overflow menu being opened.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  EXPECT_CALL(*bridge_, ConstructMenuItemArray(env, _, _, _, _, _))
      .WillOnce(Return(nullptr));
  translate_message_->BuildOverflowMenu(env);

  // Dismiss the translate message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  // The dismissal isn't counted as an ignore because opening the overflow menu
  // counts as interacting with the UI.
  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));

  // Show the translate message again, this time without interacting with it.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  // Dismiss the translate message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));
  EXPECT_EQ(2, on_dismiss_callback_called_count_);

  // The dismissal still isn't counted as an ignore because the translate
  // message was previously interacted with on this page.
  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationNotIgnoredBecauseErrorOccurred) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("fr");
  manager_->GetLanguageState()->SetCurrentLanguage("fr");

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(*bridge_,
              ShowMessage(env,
                          /*title=*/Truly(IsJavaStringNonNull),
                          /*description=*/Truly(IsJavaStringNonNull),
                          /*primary_button_text=*/Truly(IsJavaStringNonNull),
                          /*has_overflow_menu=*/true));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "fr",
                                        "en");

  // Dismiss the message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  EXPECT_EQ(0, translate_prefs_->GetTranslationIgnoredCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationDenied) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", 100);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateIgnoredCount)
      ->Set("fr", 100);

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));

  histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                      InfobarEvent::INFOBAR_DECLINE, 1);
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  EXPECT_EQ(0, translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(1, translate_prefs_->GetTranslationDeniedCount("fr"));
  EXPECT_EQ(100, translate_prefs_->GetTranslationIgnoredCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationNotDeniedBecauseOverflowMenuOpened) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Show the translate message and simulate the overflow menu being opened.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  EXPECT_CALL(*bridge_, ConstructMenuItemArray(env, _, _, _, _, _))
      .WillOnce(Return(nullptr));
  translate_message_->BuildOverflowMenu(env);

  // Dismiss the translate message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  // The dismissal isn't counted as an denial because opening the overflow menu
  // counts as interacting with the UI.
  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));

  // Show the translate message again, this time without interacting with it.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  // Dismiss the translate message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));
  EXPECT_EQ(2, on_dismiss_callback_called_count_);

  // The dismissal still isn't counted as an denial because the translate
  // message was previously interacted with on this page.
  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));
}

TEST_F(TranslateMessageTest, TranslationNotDeniedBecauseErrorOccurred) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("fr");
  manager_->GetLanguageState()->SetCurrentLanguage("fr");

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(*bridge_,
              ShowMessage(env,
                          /*title=*/Truly(IsJavaStringNonNull),
                          /*description=*/Truly(IsJavaStringNonNull),
                          /*primary_button_text=*/Truly(IsJavaStringNonNull),
                          /*has_overflow_menu=*/true));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "fr",
                                        "en");

  // Dismiss the message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);

  EXPECT_EQ(0, translate_prefs_->GetTranslationDeniedCount("fr"));
}

TEST_F(TranslateMessageTest, AutoAlwaysTranslate) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", GetAutoAlwaysThreshold() - 1);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoAlwaysCount)
      ->Set("fr", GetMaximumNumberOfAutoAlways() - 1);

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);

  {
    base::HistogramTester histogram_tester;
    FinishTranslation(env, "fr", "en");
    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram,
        InfobarEvent::INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION, 1);
  }

  EXPECT_TRUE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
  EXPECT_EQ(GetMaximumNumberOfAutoAlways(),
            translate_prefs_->GetTranslationAutoAlwaysCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationAcceptedCount("fr"));

  {
    base::HistogramTester histogram_tester;
    // Simulate clicking "Undo", which should disable "Always translate
    // language."
    ExpectTranslationReverts(env, "fr", "en");
    translate_message_->HandlePrimaryAction(env);

    histogram_tester.ExpectBucketCount(
        kInfobarEventHistogram,
        InfobarEvent::INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS, 1);
    histogram_tester.ExpectBucketCount(kInfobarEventHistogram,
                                       InfobarEvent::INFOBAR_REVERT, 1);
    histogram_tester.ExpectTotalCount(kInfobarEventHistogram, 2);
  }

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
}

TEST_F(TranslateMessageTest, AutoAlwaysTranslatePastAcceptedThreshold) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Once the translation happens, this count will be 1 larger than the
  // threshold. This test is to make sure that the comparison is
  // greater-than-or-equal, not just equality.
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", GetAutoAlwaysThreshold());
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoAlwaysCount)
      ->Set("fr", GetMaximumNumberOfAutoAlways() - 1);

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  FinishTranslation(env, "fr", "en");

  EXPECT_TRUE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
  EXPECT_EQ(GetMaximumNumberOfAutoAlways(),
            translate_prefs_->GetTranslationAutoAlwaysCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationAcceptedCount("fr"));
}

TEST_F(TranslateMessageTest, AutoAlwaysTranslateDismissedInProgress) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", GetAutoAlwaysThreshold() - 1);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoAlwaysCount)
      ->Set("fr", GetMaximumNumberOfAutoAlways() - 1);

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  // Start the translation.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);

  // Simulate the message being dismissed from Java.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  // Finish the translation, causing the Message to pop up again.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  FinishTranslation(env, "fr", "en");

  EXPECT_TRUE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
  EXPECT_EQ(GetMaximumNumberOfAutoAlways(),
            translate_prefs_->GetTranslationAutoAlwaysCount("fr"));
  EXPECT_EQ(0, translate_prefs_->GetTranslationAcceptedCount("fr"));

  // Simulate clicking "Undo", which should disable "Always translate language."
  ExpectTranslationReverts(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
}

TEST_F(TranslateMessageTest, AutoAlwaysTranslateThresholdNotReached) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", GetAutoAlwaysThreshold() - 2);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoAlwaysCount)
      ->Set("fr", GetMaximumNumberOfAutoAlways() - 1);

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  // Start the translation.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  FinishTranslation(env, "fr", "en");

  // Auto-always-translate should not have triggered, since the threshold of
  // accepted translations has not been reached.
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
  EXPECT_EQ(GetAutoAlwaysThreshold() - 1,
            translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(GetMaximumNumberOfAutoAlways() - 1,
            translate_prefs_->GetTranslationAutoAlwaysCount("fr"));
}

TEST_F(TranslateMessageTest, AutoAlwaysTranslatePastMaximumTimes) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", GetAutoAlwaysThreshold() - 1);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoAlwaysCount)
      ->Set("fr", GetMaximumNumberOfAutoAlways());

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  // Start the translation.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  FinishTranslation(env, "fr", "en");

  // Auto-always-translate should not have triggered, since
  // auto-always-translate has already triggered the maximum number of allowed
  // times.
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
  EXPECT_EQ(GetAutoAlwaysThreshold(),
            translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(GetMaximumNumberOfAutoAlways(),
            translate_prefs_->GetTranslationAutoAlwaysCount("fr"));
}

TEST_F(TranslateMessageTest, AutoAlwaysTranslateInterruptedByOverflowMenu) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAcceptedCount)
      ->Set("fr", GetAutoAlwaysThreshold() - 1);
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoAlwaysCount)
      ->Set("fr", GetMaximumNumberOfAutoAlways() - 1);

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  // Start the translation.
  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ON_CALL(*client_, IsTranslatableURL(_)).WillByDefault(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);

  // While the translation is in progress, simulate the overflow menu being
  // opened.
  EXPECT_CALL(*bridge_, ConstructMenuItemArray(env, _, _, _, _, _))
      .WillOnce(Return(nullptr));
  translate_message_->BuildOverflowMenu(env);

  FinishTranslation(env, "fr", "en");

  // Auto-always-translate should not have triggered, since opening the overflow
  // menu mid-translation should have prevented it from triggering.
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
  EXPECT_EQ(GetAutoAlwaysThreshold(),
            translate_prefs_->GetTranslationAcceptedCount("fr"));
  EXPECT_EQ(GetMaximumNumberOfAutoAlways() - 1,
            translate_prefs_->GetTranslationAutoAlwaysCount("fr"));
}

TEST_F(TranslateMessageTest, AutoNeverTranslate) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateDeniedCount)
      ->Set("fr", GetAutoNeverThreshold() - 1);

  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  {
    base::HistogramTester histogram_tester;

    // Dismissing the message should cause the auto-never-translate confirmation
    // message to be shown.
    EXPECT_CALL(*bridge_,
                ShowMessage(env,
                            /*title=*/Truly(IsJavaStringNonNull),
                            /*description=*/Truly(IsJavaStringNull),
                            /*primary_button_text=*/Truly(IsJavaStringNonNull),
                            /*has_overflow_menu=*/false));
    translate_message_->HandleDismiss(
        env, static_cast<jint>(messages::DismissReason::GESTURE));

    histogram_tester.ExpectBucketCount(
        kInfobarEventHistogram,
        InfobarEvent::INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION, 1);
    histogram_tester.ExpectBucketCount(kInfobarEventHistogram,
                                       InfobarEvent::INFOBAR_DECLINE, 1);
    histogram_tester.ExpectTotalCount(kInfobarEventHistogram, 2);
  }

  // The dismissal callback should not have been run.
  EXPECT_EQ(0, on_dismiss_callback_called_count_);

  EXPECT_TRUE(translate_prefs_->IsBlockedLanguage("fr"));

  {
    base::HistogramTester histogram_tester;
    // Click "Undo" on the confirmation.
    EXPECT_CALL(*bridge_, Dismiss(env))
        .WillOnce(InvokeWithoutArgs([env,
                                     message = translate_message_.get()]() {
          message->HandleDismiss(
              env,
              static_cast<jint>(messages::DismissReason::DISMISSED_BY_FEATURE));
        }));
    EXPECT_CALL(*bridge_, ClearNativePointer(env));
    translate_message_->HandlePrimaryAction(env);

    histogram_tester.ExpectUniqueSample(
        kInfobarEventHistogram,
        InfobarEvent::INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER, 1);
  }

  EXPECT_EQ(1, on_dismiss_callback_called_count_);
  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));
}

TEST_F(TranslateMessageTest, AutoNeverTranslatePastMaximumTimes) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateDeniedCount)
      ->Set("fr", GetAutoNeverThreshold());
  ScopedDictPrefUpdate(pref_service_.get(),
                       TranslatePrefs::kPrefTranslateAutoNeverCount)
      ->Set("fr", GetMaximumNumberOfAutoNever());

  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds))
      .WillOnce(Return(true));
  ShowBeforeTranslationMessage(env, "fr", "en");

  // Dismiss the message.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::GESTURE));
  EXPECT_EQ(1, on_dismiss_callback_called_count_);
  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));
}

}  // namespace
}  // namespace translate

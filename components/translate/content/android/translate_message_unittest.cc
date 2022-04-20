// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_message.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/translate/core/common/translate_constants.h"
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

  MOCK_METHOD(void,
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
               base::android::ScopedJavaLocalRef<jstring>),
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
  MOCK_METHOD(bool, IsIncognito, (), (override));
  MOCK_METHOD(bool, HasCurrentPage, (), (override));

  const GURL& GetLastCommittedURL() override { return url_; }
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

void OnDismissTranslateMessage(
    std::unique_ptr<TranslateMessage>* translate_message,
    bool* was_on_dismiss_callback_called) {
  *was_on_dismiss_callback_called = true;
  translate_message->reset();
}

constexpr int kDefaultDismissalDurationSeconds = 10;

struct SecondaryMenuItem {
  TranslateMessage::OverflowMenuItemId id;
  bool has_checkmark;
  std::string language_code;
};

class TranslateMessageTest : public ::testing::Test {
 public:
  TranslateMessageTest() {
    ON_CALL(driver_, IsIncognito()).WillByDefault(Return(false));
    ON_CALL(driver_, HasCurrentPage()).WillByDefault(Return(true));
    driver_.SetLastCommittedURL(GURL("http://www.example.com/"));
  }

  void CreateAndShowBeforeTranslationMessage(
      JNIEnv* env,
      const std::string& source_language_code,
      const std::string& target_language_code) {
    EXPECT_CALL(*bridge_, CreateTranslateMessage(
                              env, _, _, kDefaultDismissalDurationSeconds));

    EXPECT_CALL(
        *bridge_,
        ShowMessage(env,
                    /*title=*/Truly(IsJavaStringNonNull),
                    /*description=*/Truly(IsJavaStringNonNull),
                    /*primary_button_text=*/Truly(IsJavaStringNonNull)));

    translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE,
                                          source_language_code,
                                          target_language_code);

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
  }

  void ExpectTranslationInProgress(JNIEnv* env,
                                   const std::string& source_language_code,
                                   const std::string& target_language_code) {
    EXPECT_CALL(*bridge_,
                ShowMessage(env,
                            /*title=*/Truly(IsJavaStringNonNull),
                            /*description=*/Truly(IsJavaStringNonNull),
                            /*primary_button_text=*/Truly(IsJavaStringNull)));

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
    EXPECT_CALL(
        *bridge_,
        ShowMessage(env,
                    /*title=*/Truly(IsJavaStringNonNull),
                    /*description=*/Truly(IsJavaStringNonNull),
                    /*primary_button_text=*/Truly(IsJavaStringNonNull)));

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

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
  }

  void ExpectTranslationReverts(JNIEnv* env,
                                const std::string& source_language_code,
                                const std::string& target_language_code) {
    EXPECT_CALL(driver_, RevertTranslation(_));

    EXPECT_CALL(
        *bridge_,
        ShowMessage(env,
                    /*title=*/Truly(IsJavaStringNonNull),
                    /*description=*/Truly(IsJavaStringNonNull),
                    /*primary_button_text=*/Truly(IsJavaStringNonNull)));
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
              return std::equal(expected_items.begin(), expected_items.end(),
                                actual_vector.begin(), actual_vector.end(),
                                [](const SecondaryMenuItem& lhs, bool rhs) {
                                  return lhs.has_checkmark == rhs;
                                });
            }),
            /*overflow_menu_item_ids=*/
            Truly([env, expected_items](
                      const base::android::ScopedJavaLocalRef<jintArray>&
                          actual) {
              std::vector<int> actual_vector;
              base::android::JavaIntArrayToIntVector(env, actual,
                                                     &actual_vector);
              return std::equal(expected_items.begin(), expected_items.end(),
                                actual_vector.begin(), actual_vector.end(),
                                [](const SecondaryMenuItem& lhs, int rhs) {
                                  return static_cast<int>(lhs.id) == rhs;
                                });
            }),
            /*language_codes=*/
            Truly([env, expected_items](
                      const base::android::ScopedJavaLocalRef<jobjectArray>&
                          actual) {
              std::vector<std::string> actual_vector;
              base::android::AppendJavaStringArrayToStringVector(
                  env, actual, &actual_vector);
              return std::equal(
                  expected_items.begin(), expected_items.end(),
                  actual_vector.begin(), actual_vector.end(),
                  [](const SecondaryMenuItem& lhs, const std::string& rhs) {
                    return lhs.language_code == rhs;
                  });
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
        base::BindOnce(&OnDismissTranslateMessage, &translate_message_,
                       &was_on_dismiss_callback_called_),
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
  bool was_on_dismiss_callback_called_ = false;
};

TEST_F(TranslateMessageTest, TranslateAndRevert) {
  JNIEnv* env = base::android::AttachCurrentThread();

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  FinishTranslation(env, "fr", "en");

  ExpectTranslationReverts(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  // Simulate a dismissal triggered from the Java side.
  EXPECT_CALL(*bridge_, ClearNativePointer(env));
  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));

  // The on-dismiss callback should have been called.
  EXPECT_FALSE(translate_message_);
  EXPECT_TRUE(was_on_dismiss_callback_called_);
}

TEST_F(TranslateMessageTest, ShowErrorBeforeTranslation) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("fr");
  manager_->GetLanguageState()->SetCurrentLanguage("fr");
  ASSERT_TRUE(Mock::VerifyAndClear(client_.get()));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(*bridge_,
              ShowMessage(env,
                          /*title=*/Truly(IsJavaStringNonNull),
                          /*description=*/Truly(IsJavaStringNonNull),
                          /*primary_button_text=*/Truly(IsJavaStringNonNull)));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "fr",
                                        "en");
}

TEST_F(TranslateMessageTest, ShowErrorAfterTranslation) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("fr");
  manager_->GetLanguageState()->SetCurrentLanguage("en");
  ASSERT_TRUE(Mock::VerifyAndClear(client_.get()));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(*bridge_,
              ShowMessage(env,
                          /*title=*/Truly(IsJavaStringNonNull),
                          /*description=*/Truly(IsJavaStringNonNull),
                          /*primary_button_text=*/Truly(IsJavaStringNonNull)));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "fr",
                                        "en");
}

TEST_F(TranslateMessageTest, DismissMessageOnDestruction) {
  JNIEnv* env = base::android::AttachCurrentThread();

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  EXPECT_FALSE(was_on_dismiss_callback_called_);
}

TEST_F(TranslateMessageTest, OverflowMenuToggleAlwaysTranslateLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));

  ExpectTranslationInProgress(env, "fr", "en");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kToggleAlwaysTranslateLanguage),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kToggleAlwaysTranslateLanguage),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(true)));

  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("fr", "en"));
}

TEST_F(TranslateMessageTest, OverflowMenuToggleNeverTranslateLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Begin from a translated page.
  CreateAndShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));

  ExpectTranslationReverts(env, "fr", "en");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateLanguage),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(true)));
  EXPECT_FALSE(translate_prefs_->IsBlockedLanguage("fr"));
}

TEST_F(TranslateMessageTest, OverflowMenuToggleNeverTranslateSite) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Begin from a translated page.
  CreateAndShowBeforeTranslationMessage(env, "fr", "en");
  ExpectTranslationInProgress(env, "fr", "en");
  translate_message_->HandlePrimaryAction(env);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList(
      driver_.GetLastCommittedURL().HostNoBracketsPiece()));

  ExpectTranslationReverts(env, "fr", "en");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kToggleNeverTranslateSite),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(true)));
  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList(
      driver_.GetLastCommittedURL().HostNoBracketsPiece()));
}

TEST_F(TranslateMessageTest, OverflowMenuChangeSourceLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  translate_prefs_->AddToLanguageList("en", true);
  translate_prefs_->AddToLanguageList("es", true);
  translate_prefs_->AddToLanguageList("de", true);

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  std::vector<SecondaryMenuItem> menu_items;
  TranslateUIDelegate ui_delegate(manager_->GetWeakPtr(), "fr", "en");
  for (size_t i = 0U; i < ui_delegate.GetNumberOfLanguages(); ++i) {
    std::string language_code = ui_delegate.GetLanguageCodeAt(i);
    if (language_code == "fr")
      continue;
    menu_items.emplace_back(SecondaryMenuItem{
        TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage, false,
        std::move(language_code)});
  }

  // Click the kChangeSourceLanguage option in the overflow menu, which should
  // return a list of language picker menu items.
  ExpectConstructMenuItemArray(env, menu_items, CreateTestJobjectArray(env));
  EXPECT_TRUE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage),
      base::android::ConvertUTF8ToJavaString(env, std::string()),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  // Clicking a language should kick off a translation.
  ExpectTranslationInProgress(env, "de", "en");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeSourceLanguage),
      base::android::ConvertUTF8ToJavaString(env, "de"),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

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

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  std::vector<SecondaryMenuItem> menu_items;
  TranslateUIDelegate ui_delegate(manager_->GetWeakPtr(), "fr", "en");
  for (size_t i = 0U; i < ui_delegate.GetNumberOfLanguages(); ++i) {
    std::string language_code = ui_delegate.GetLanguageCodeAt(i);
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  // Clicking a language should kick off a translation.
  ExpectTranslationInProgress(env, "fr", "de");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage),
      base::android::ConvertUTF8ToJavaString(env, "de"),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

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

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  std::vector<SecondaryMenuItem> menu_items = {
      {TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
       "es"},
      {TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage, false,
       "de"},
      // Divider after content languages.
      {TranslateMessage::OverflowMenuItemId::kInvalid, false, std::string()},
  };

  TranslateUIDelegate ui_delegate(manager_->GetWeakPtr(), "fr", "en");
  for (size_t i = 0U; i < ui_delegate.GetNumberOfLanguages(); ++i) {
    std::string language_code = ui_delegate.GetLanguageCodeAt(i);
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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  // Clicking a language should kick off a translation.
  ExpectTranslationInProgress(env, "fr", "de");
  EXPECT_FALSE(translate_message_->HandleSecondaryMenuItemClicked(
      env,
      static_cast<int>(
          TranslateMessage::OverflowMenuItemId::kChangeTargetLanguage),
      base::android::ConvertUTF8ToJavaString(env, "de"),
      static_cast<jboolean>(false)));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  FinishTranslation(env, "fr", "de");
}

TEST_F(TranslateMessageTest, OverflowMenuIncognito) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ON_CALL(driver_, IsIncognito()).WillByDefault(Return(true));

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
}

TEST_F(TranslateMessageTest, OverflowMenuEmptyUrl) {
  JNIEnv* env = base::android::AttachCurrentThread();
  driver_.SetLastCommittedURL(GURL());

  CreateAndShowBeforeTranslationMessage(env, "fr", "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
}

TEST_F(TranslateMessageTest, OverflowMenuUnknownSourceLanguage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  CreateAndShowBeforeTranslationMessage(env, kUnknownLanguageCode, "en");

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
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));
}

}  // namespace
}  // namespace translate

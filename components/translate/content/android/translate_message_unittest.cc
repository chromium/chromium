// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_message.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
              ShowBeforeTranslateMessage,
              (JNIEnv*,
               base::android::ScopedJavaLocalRef<jstring>,
               base::android::ScopedJavaLocalRef<jstring>),
              (override));
  MOCK_METHOD(void,
              ShowTranslationInProgressMessage,
              (JNIEnv*,
               base::android::ScopedJavaLocalRef<jstring>,
               base::android::ScopedJavaLocalRef<jstring>),
              (override));
  MOCK_METHOD(void,
              ShowAfterTranslateMessage,
              (JNIEnv*,
               base::android::ScopedJavaLocalRef<jstring>,
               base::android::ScopedJavaLocalRef<jstring>),
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
};

TestTranslateDriver::~TestTranslateDriver() = default;

std::u16string GetLanguageDisplayName(const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(language_code, "en", true);
}

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

void SetBoolToTrue(bool* ptr) {
  *ptr = true;
}

constexpr int kDefaultDismissalDurationSeconds = 10;

class TranslateMessageTest : public ::testing::Test {
 public:
  TranslateMessageTest() = default;

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

    auto owned_bridge = std::make_unique<TestBridge>();
    bridge_ = owned_bridge->GetWeakPtr();
    translate_message_ = std::make_unique<TranslateMessage>(
        /*web_contents=*/nullptr, manager_->GetWeakPtr(),
        base::BindOnce(&SetBoolToTrue, &was_on_dismiss_callback_called_),
        std::move(owned_bridge));
  }

  TestTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<TestLanguageModel> language_model_;
  std::unique_ptr<TranslateManager> manager_;
  std::unique_ptr<MockTranslateRanker> ranker_;

  bool was_on_dismiss_callback_called_ = false;
  base::WeakPtr<TestBridge> bridge_;
  std::unique_ptr<TranslateMessage> translate_message_;
};

TEST_F(TranslateMessageTest, TranslateAndRevert) {
  JNIEnv* env = base::android::AttachCurrentThread();

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds));

  EXPECT_CALL(
      *bridge_,
      ShowBeforeTranslateMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE, "en",
                                        "fr");

  EXPECT_FALSE(was_on_dismiss_callback_called_);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_CALL(
      *bridge_,
      ShowTranslationInProgressMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  EXPECT_CALL(*client_, ShowTranslateUI(TRANSLATE_STEP_TRANSLATING, "en", "fr",
                                        TranslateErrors::NONE, false))
      .WillOnce(DoAll(InvokeWithoutArgs([this]() {
                        translate_message_->ShowTranslateStep(
                            TRANSLATE_STEP_TRANSLATING, "en", "fr");
                      }),
                      Return(true)));

  translate_message_->HandlePrimaryAction(env);

  EXPECT_FALSE(was_on_dismiss_callback_called_);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_CALL(
      *bridge_,
      ShowAfterTranslateMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  EXPECT_CALL(*client_, ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, "en",
                                        "fr", TranslateErrors::NONE, false))
      .WillOnce(DoAll(InvokeWithoutArgs([this]() {
                        translate_message_->ShowTranslateStep(
                            TRANSLATE_STEP_AFTER_TRANSLATE, "en", "fr");
                      }),
                      Return(true)));

  manager_->PageTranslated("en", "fr", TranslateErrors::NONE);

  EXPECT_FALSE(was_on_dismiss_callback_called_);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_CALL(driver_, RevertTranslation(_));

  EXPECT_CALL(
      *bridge_,
      ShowBeforeTranslateMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  translate_message_->HandlePrimaryAction(env);

  EXPECT_FALSE(was_on_dismiss_callback_called_);
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(bridge_.get()));

  EXPECT_CALL(*bridge_, ClearNativePointer(env));

  translate_message_->HandleDismiss(
      env, static_cast<jint>(messages::DismissReason::TIMER));

  EXPECT_TRUE(was_on_dismiss_callback_called_);
}

TEST_F(TranslateMessageTest, ShowErrorBeforeTranslation) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("en");
  manager_->GetLanguageState()->SetCurrentLanguage("en");
  ASSERT_TRUE(Mock::VerifyAndClear(client_.get()));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(
      *bridge_,
      ShowBeforeTranslateMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "en",
                                        "fr");
}

TEST_F(TranslateMessageTest, ShowErrorAfterTranslation) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ON_CALL(*client_, ShowTranslateUI(_, _, _, _, _)).WillByDefault(Return(true));
  manager_->GetLanguageState()->SetSourceLanguage("en");
  manager_->GetLanguageState()->SetCurrentLanguage("fr");
  ASSERT_TRUE(Mock::VerifyAndClear(client_.get()));

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds));
  EXPECT_CALL(*bridge_, ShowTranslateError(env, _));
  EXPECT_CALL(
      *bridge_,
      ShowAfterTranslateMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_TRANSLATE_ERROR, "en",
                                        "fr");
}

TEST_F(TranslateMessageTest, DismissMessageOnDestruction) {
  JNIEnv* env = base::android::AttachCurrentThread();

  EXPECT_CALL(*bridge_, CreateTranslateMessage(
                            env, _, _, kDefaultDismissalDurationSeconds));
  EXPECT_CALL(
      *bridge_,
      ShowBeforeTranslateMessage(
          env, Truly(JavaStringMatcher(env, GetLanguageDisplayName("en"))),
          Truly(JavaStringMatcher(env, GetLanguageDisplayName("fr")))));

  translate_message_->ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE, "en",
                                        "fr");

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

}  // namespace
}  // namespace translate

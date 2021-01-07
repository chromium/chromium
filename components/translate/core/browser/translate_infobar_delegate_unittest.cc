// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_infobar_delegate.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::Test;
using translate::testing::MockTranslateClient;
using translate::testing::MockTranslateDriver;
using translate::testing::MockTranslateRanker;

namespace translate {

const char kOriginalLanguage[] = "fr";
const char kTargetLanguage[] = "en";

namespace {

class TestInfoBarManager : public infobars::InfoBarManager {
 public:
  TestInfoBarManager() = default;
  // infobars::InfoBarManager:
  ~TestInfoBarManager() override {}

  // infobars::InfoBarManager:
  std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate) override {
    return std::make_unique<infobars::InfoBar>(std::move(delegate));
  }

  // infobars::InfoBarManager:
  int GetActiveEntryID() override { return 0; }

  // infobars::InfoBarManager:
  void OpenURL(const GURL& url, WindowOpenDisposition disposition) override {
    NOTREACHED();
  }
};

}  // namespace

class MockObserver : public TranslateInfoBarDelegate::Observer {
 public:
  MOCK_METHOD(void,
              OnTranslateInfoBarDelegateDestroyed,
              (TranslateInfoBarDelegate*),
              (override));
  MOCK_METHOD(void,
              OnTranslateStepChanged,
              (translate::TranslateStep, TranslateErrors::Type),
              (override));
  MOCK_METHOD(void, OnTargetLanguageChanged, (const std::string&), (override));
  MOCK_METHOD(bool, IsDeclinedByUser, (), (override));
};

class TestLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class TranslateInfoBarDelegateTest : public ::testing::Test {
 public:
  TranslateInfoBarDelegateTest() = default;

 protected:
  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->SetString(testing::accept_languages_prefs, std::string());
    pref_service_->SetString(language::prefs::kAcceptLanguages, std::string());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    pref_service_->SetString(language::prefs::kPreferredLanguages,
                             std::string());
#endif
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
    ranker_ = std::make_unique<MockTranslateRanker>();
    client_ =
        std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
    manager_ = std::make_unique<TranslateManager>(client_.get(), ranker_.get(),
                                                  language_model_.get());
    manager_->GetLanguageState()->set_translation_declined(false);
    infobar_manager_ = std::make_unique<TestInfoBarManager>();
  }

  std::unique_ptr<TranslateInfoBarDelegate> ConstructInfoBarDelegate() {
    return std::unique_ptr<TranslateInfoBarDelegate>(
        new TranslateInfoBarDelegate(
            manager_->GetWeakPtr(), /*is_off_the_record=*/false,
            translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
            kOriginalLanguage, kTargetLanguage, TranslateErrors::Type::NONE,
            /*triggered_from_menu=*/false));
  }

  MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<TestLanguageModel> language_model_;
  std::unique_ptr<TranslateManager> manager_;
  std::unique_ptr<MockTranslateRanker> ranker_;
  std::unique_ptr<TestInfoBarManager> infobar_manager_;
};

TEST_F(TranslateInfoBarDelegateTest, CreateTranslateInfobarDelegate) {
  EXPECT_EQ(infobar_manager_->infobar_count(), 0u);

  // Create the initial InfoBar
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(),
      /*is_off_the_record=*/false,
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATING, kOriginalLanguage,
      kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobar_count(), 1u);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_FALSE(delegate->is_error());
  EXPECT_EQ(translate::TranslateStep::TRANSLATE_STEP_TRANSLATING,
            delegate->translate_step());
  EXPECT_FALSE(delegate->is_off_the_record());
  EXPECT_FALSE(delegate->triggered_from_menu());
  EXPECT_EQ(delegate->target_language_code(), kTargetLanguage);
  EXPECT_EQ(delegate->original_language_code(), kOriginalLanguage);

  // Create another one and replace the old one
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(),
      /*is_off_the_record=*/true,
      translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE,
      kOriginalLanguage, kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobar_count(), 1u);
  delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_EQ(delegate->translate_step(),
            translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);

  // Create but don't replace existing one.
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(),
      /*is_off_the_record=*/false,
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
      kOriginalLanguage, kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobar_count(), 1u);
  delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  ASSERT_EQ(delegate->translate_step(),
            translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);
}

TEST_F(TranslateInfoBarDelegateTest, DestructTranslateInfobarDelegate) {
  MockObserver mock_observer;
  std::unique_ptr<TranslateInfoBarDelegate> delegate =
      ConstructInfoBarDelegate();
  EXPECT_CALL(mock_observer,
              OnTranslateInfoBarDelegateDestroyed(delegate.get()));

  delegate->AddObserver(&mock_observer);
  delegate.reset();
}

TEST_F(TranslateInfoBarDelegateTest, IsTranslatableLanguage) {
  // A language is translatable if it's not blocked or is not an accept
  // language.
  std::unique_ptr<TranslateInfoBarDelegate> delegate =
      ConstructInfoBarDelegate();
  TranslateAcceptLanguages accept_languages(pref_service_.get(),
                                            testing::accept_languages_prefs);
  ON_CALL(*(client_.get()), GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_languages));
  ListPrefUpdate update(pref_service_.get(), language::prefs::kFluentLanguages);
  update->Append(kOriginalLanguage);
  pref_service_->SetString(language::prefs::kAcceptLanguages,
                           kOriginalLanguage);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  pref_service_->SetString(language::prefs::kPreferredLanguages,
                           kOriginalLanguage);
#endif

  EXPECT_FALSE(delegate->IsTranslatableLanguageByPrefs());

  // Remove kOriginalLanguage from the blocked languages.
  update->EraseListValue(base::Value(kOriginalLanguage));
  EXPECT_TRUE(delegate->IsTranslatableLanguageByPrefs());
}

TEST_F(TranslateInfoBarDelegateTest, ShouldAutoAlwaysTranslate) {
  DictionaryPrefUpdate update_translate_accepted_count(
      pref_service_.get(), TranslatePrefs::kPrefTranslateAcceptedCount);
  base::DictionaryValue* update_translate_accepted_dict =
      update_translate_accepted_count.Get();
  // 6 = kAutoAlwaysThreshold + 1
  update_translate_accepted_dict->SetInteger(kOriginalLanguage, 6);

  const base::DictionaryValue* dict = pref_service_->GetDictionary(
      TranslatePrefs::kPrefTranslateAutoAlwaysCount);
  int translate_auto_always_count = 0;
  dict->GetInteger(kOriginalLanguage, &translate_auto_always_count);
  EXPECT_EQ(0, translate_auto_always_count);

  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(),
      /*is_off_the_record=*/false,
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATING, kOriginalLanguage,
      kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_TRUE(delegate->ShouldAutoAlwaysTranslate());

  int count = -1;
  update_translate_accepted_dict->GetInteger(kOriginalLanguage, &count);
  EXPECT_EQ(0, count);
  dict = pref_service_->GetDictionary(
      TranslatePrefs::kPrefTranslateAutoAlwaysCount);
  translate_auto_always_count = 0;
  dict->GetInteger(kOriginalLanguage, &translate_auto_always_count);
  EXPECT_EQ(1, translate_auto_always_count);
}

TEST_F(TranslateInfoBarDelegateTest, ShouldNotAutoAlwaysTranslate) {
  // Create an off record info bar.
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), /*is_off_the_record=*/true,
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATING, kOriginalLanguage,
      kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobar_count(), 1u);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_FALSE(delegate->ShouldAutoAlwaysTranslate());
}

TEST_F(TranslateInfoBarDelegateTest, ShouldAutoNeverTranslate) {
  TranslateAcceptLanguages accept_languages(pref_service_.get(),
                                            testing::accept_languages_prefs);
  ON_CALL(*(client_.get()), GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_languages));

  DictionaryPrefUpdate update_translate_denied_count(
      pref_service_.get(), TranslatePrefs::kPrefTranslateDeniedCount);
  base::DictionaryValue* update_translate_denied_dict =
      update_translate_denied_count.Get();
  // 21 = kAutoNeverThreshold + 1
  update_translate_denied_dict->SetInteger(kOriginalLanguage, 21);

  const base::DictionaryValue* dict = pref_service_->GetDictionary(
      TranslatePrefs::kPrefTranslateAutoNeverCount);
  int translate_auto_never_count = 0;
  dict->GetInteger(kOriginalLanguage, &translate_auto_never_count);
  EXPECT_EQ(0, translate_auto_never_count);

  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(),
      /*is_off_the_record=*/false,
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATING, kOriginalLanguage,
      kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_TRUE(delegate->ShouldAutoNeverTranslate());

  int count = -1;
  update_translate_denied_dict->GetInteger(kOriginalLanguage, &count);
  EXPECT_EQ(0, count);
  dict = pref_service_->GetDictionary(
      TranslatePrefs::kPrefTranslateAutoNeverCount);
  translate_auto_never_count = 0;
  dict->GetInteger(kOriginalLanguage, &translate_auto_never_count);
  EXPECT_EQ(1, translate_auto_never_count);
}

TEST_F(TranslateInfoBarDelegateTest, ShouldAutoNeverTranslate_Not) {
  // Create an off record info bar.
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), /*is_off_the_record=*/true,
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATING, kOriginalLanguage,
      kTargetLanguage, TranslateErrors::Type::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobar_count(), 1u);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobar_at(0)->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_FALSE(delegate->ShouldAutoNeverTranslate());
}

}  // namespace translate

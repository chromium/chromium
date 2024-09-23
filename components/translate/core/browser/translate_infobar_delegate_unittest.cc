// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_infobar_delegate.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

namespace {

using ::testing::_;
using testing::MockTranslateClient;
using testing::MockTranslateDriver;
using testing::MockTranslateRanker;
using ::testing::Return;
using ::testing::Test;

const int kAutoAlwaysThreshold = 5;
const char kSourceLanguage[] = "fr";
const char kTargetLanguage[] = "en";

class TestInfoBarManager : public infobars::InfoBarManager {
 public:
  TestInfoBarManager() = default;
  // infobars::InfoBarManager:
  ~TestInfoBarManager() override {}

  // infobars::InfoBarManager:
  int GetActiveEntryID() override { return 0; }

  // infobars::InfoBarManager:
  void OpenURL(const GURL& url, WindowOpenDisposition disposition) override {
    NOTREACHED_IN_MIGRATION();
  }
};

class MockObserver : public TranslateInfoBarDelegate::Observer {
 public:
  MOCK_METHOD(void,
              OnTranslateInfoBarDelegateDestroyed,
              (TranslateInfoBarDelegate*),
              (override));
  MOCK_METHOD(void,
              OnTranslateStepChanged,
              (TranslateStep, TranslateErrors),
              (override));
  MOCK_METHOD(void, OnTargetLanguageChanged, (const std::string&), (override));
  MOCK_METHOD(bool, IsDeclinedByUser, (), (override));
};

class MockManagerObserver : public infobars::InfoBarManager::Observer {
 public:
  MOCK_METHOD(void, OnInfoBarAdded, (infobars::InfoBar*), (override));
  MOCK_METHOD(void, OnInfoBarRemoved, (infobars::InfoBar*, bool), (override));
  MOCK_METHOD(void,
              OnInfoBarReplaced,
              (infobars::InfoBar*, infobars::InfoBar*),
              (override));
  MOCK_METHOD(void,
              OnManagerShuttingDown,
              (infobars::InfoBarManager*),
              (override));
};

class TestLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

}  // namespace

class TranslateInfoBarDelegateTest : public ::testing::Test {
 public:
  TranslateInfoBarDelegateTest() = default;

 protected:
  void SetUp() override {
    ::testing::Test::SetUp();
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

  void TearDown() override {
    infobar_manager_->ShutDown();
    ::testing::Test::TearDown();
  }

  std::unique_ptr<TranslateInfoBarDelegate> ConstructInfoBarDelegate() {
    return std::unique_ptr<TranslateInfoBarDelegate>(
        new TranslateInfoBarDelegate(
            manager_->GetWeakPtr(),
            TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE, kSourceLanguage,
            kTargetLanguage, TranslateErrors::NONE,
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
  ::testing::StrictMock<MockObserver> mock_observer;
  ::testing::StrictMock<MockManagerObserver> mock_manager_observer;
  EXPECT_EQ(infobar_manager_->infobars().size(), 0u);
  infobar_manager_->AddObserver(&mock_manager_observer);

  EXPECT_CALL(mock_manager_observer, OnInfoBarAdded(_));
  // Create the initial InfoBar
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  delegate->AddObserver(&mock_observer);
  EXPECT_FALSE(delegate->is_error());
  EXPECT_EQ(TranslateStep::TRANSLATE_STEP_TRANSLATING,
            delegate->translate_step());
  EXPECT_FALSE(delegate->triggered_from_menu());
  EXPECT_EQ(delegate->target_language_code(), kTargetLanguage);
  EXPECT_EQ(delegate->source_language_code(), kSourceLanguage);

  EXPECT_CALL(mock_observer, OnTargetLanguageChanged("en"));
  EXPECT_CALL(mock_observer, OnTranslateStepChanged(
                                 TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE,
                                 TranslateErrors::NONE));
  // Create another one and replace the old one
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate_after =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();

  EXPECT_EQ(delegate_after->translate_step(),
            TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);

  // Create but don't replace existing one.
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate_final =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  ASSERT_EQ(delegate_final->translate_step(),
            TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);
  delegate->RemoveObserver(&mock_observer);
  infobar_manager_->RemoveObserver(&mock_manager_observer);
}

TEST_F(TranslateInfoBarDelegateTest, CreateTranslateInfobarDelegateFromMenu) {
  ::testing::StrictMock<MockObserver> mock_observer;
  ::testing::StrictMock<MockManagerObserver> mock_manager_observer;
  EXPECT_EQ(infobar_manager_->infobars().size(), 0u);
  infobar_manager_->AddObserver(&mock_manager_observer);

  EXPECT_CALL(mock_manager_observer, OnInfoBarAdded(_));
  // Create the initial InfoBar
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/true);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  infobars::InfoBar* infobar = infobar_manager_->infobars()[0];
  TranslateInfoBarDelegate* delegate =
      infobar->delegate()->AsTranslateInfoBarDelegate();
  delegate->AddObserver(&mock_observer);
  EXPECT_FALSE(delegate->is_error());
  EXPECT_EQ(TranslateStep::TRANSLATE_STEP_TRANSLATING,
            delegate->translate_step());
  EXPECT_TRUE(delegate->triggered_from_menu());
  EXPECT_EQ(delegate->target_language_code(), kTargetLanguage);
  EXPECT_EQ(delegate->source_language_code(), kSourceLanguage);

  // Create another one and replace the old one. As "triggered_from_menu", the
  // previous infobar will be removed and a new will be created.
  EXPECT_CALL(mock_manager_observer, OnInfoBarRemoved(infobar, true));
  EXPECT_CALL(mock_observer, OnTranslateInfoBarDelegateDestroyed(delegate));
  EXPECT_CALL(mock_manager_observer, OnInfoBarAdded(_));
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/true);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate_after =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  delegate_after->AddObserver(&mock_observer);

  EXPECT_EQ(delegate_after->translate_step(),
            TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);

  // Create but don't replace existing one.
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/true);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate_final =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  ASSERT_EQ(delegate_final->translate_step(),
            TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);

  delegate_after->RemoveObserver(&mock_observer);
  infobar_manager_->RemoveObserver(&mock_manager_observer);
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
  language::AcceptLanguagesService accept_languages(
      pref_service_.get(), testing::accept_languages_prefs);
  ON_CALL(*(client_.get()), GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ScopedListPrefUpdate update(pref_service_.get(),
                              translate::prefs::kBlockedLanguages);
  base::Value::List& update_list = update.Get();
  update_list.Append(kSourceLanguage);
  pref_service_->SetString(language::prefs::kAcceptLanguages, kSourceLanguage);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  pref_service_->SetString(language::prefs::kPreferredLanguages,
                           kSourceLanguage);
#endif

  EXPECT_FALSE(delegate->IsTranslatableLanguageByPrefs());

  // Remove kSourceLanguage from the blocked languages.
  update_list.EraseValue(base::Value(kSourceLanguage));
  EXPECT_TRUE(delegate->IsTranslatableLanguageByPrefs());
}

TEST_F(TranslateInfoBarDelegateTest, ShouldAutoAlwaysTranslate) {
  ScopedDictPrefUpdate update_translate_accepted_count(
      pref_service_.get(), TranslatePrefs::kPrefTranslateAcceptedCount);
  base::Value::Dict& update_translate_accepted_dict =
      update_translate_accepted_count.Get();
  update_translate_accepted_dict.Set(kSourceLanguage, kAutoAlwaysThreshold + 1);

  const base::Value::Dict* dict =
      &pref_service_->GetDict(TranslatePrefs::kPrefTranslateAutoAlwaysCount);
  std::optional<int> translate_auto_always_count =
      dict->FindInt(kSourceLanguage);
  EXPECT_FALSE(translate_auto_always_count.has_value());

  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_TRUE(delegate->ShouldAutoAlwaysTranslate());

  std::optional<int> count =
      update_translate_accepted_dict.FindInt(kSourceLanguage);
  EXPECT_EQ(std::optional<int>(0), count);
  // Get the dictionary again in order to update it.
  dict = &pref_service_->GetDict(TranslatePrefs::kPrefTranslateAutoAlwaysCount);
  translate_auto_always_count = dict->FindInt(kSourceLanguage);
  EXPECT_EQ(std::optional<int>(1), translate_auto_always_count);
}

TEST_F(TranslateInfoBarDelegateTest, ShouldNotAutoAlwaysTranslateUnknown) {
  ScopedDictPrefUpdate update_translate_accepted_count(
      pref_service_.get(), TranslatePrefs::kPrefTranslateAcceptedCount);
  base::Value::Dict& update_translate_accepted_dict =
      update_translate_accepted_count.Get();
  // Should not trigger auto always translate for unknown source language.
  update_translate_accepted_dict.Set(kUnknownLanguageCode,
                                     kAutoAlwaysThreshold + 1);

  const base::Value::Dict* dict =
      &pref_service_->GetDict(TranslatePrefs::kPrefTranslateAutoAlwaysCount);
  std::optional<int> translate_auto_always_count =
      dict->FindInt(kUnknownLanguageCode);
  EXPECT_FALSE(translate_auto_always_count.has_value());

  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kUnknownLanguageCode, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_FALSE(delegate->ShouldAutoAlwaysTranslate());

  std::optional<int> count =
      update_translate_accepted_dict.FindInt(kSourceLanguage);
  // Always translate not triggered, so count should be unchanged.
  EXPECT_FALSE(count.has_value());
  // Get the dictionary again in order to update it.
  dict = &pref_service_->GetDict(TranslatePrefs::kPrefTranslateAutoAlwaysCount);
  translate_auto_always_count = dict->FindInt(kUnknownLanguageCode);
  EXPECT_FALSE(translate_auto_always_count.has_value());
}

TEST_F(TranslateInfoBarDelegateTest, ShouldNotAutoAlwaysTranslate) {
  // Simulate being off-the-record.
  driver_.set_incognito();
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_FALSE(delegate->ShouldAutoAlwaysTranslate());
}

TEST_F(TranslateInfoBarDelegateTest, ShouldAutoNeverTranslate) {
  language::AcceptLanguagesService accept_languages(
      pref_service_.get(), testing::accept_languages_prefs);
  ON_CALL(*(client_.get()), GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));

  ScopedDictPrefUpdate update_translate_denied_count(
      pref_service_.get(), TranslatePrefs::kPrefTranslateDeniedCount);
  base::Value::Dict& update_translate_denied_dict =
      update_translate_denied_count.Get();
  // 21 = kAutoNeverThreshold + 1
  update_translate_denied_dict.Set(kSourceLanguage, 21);

  const base::Value::Dict* dict =
      &pref_service_->GetDict(TranslatePrefs::kPrefTranslateAutoNeverCount);
  std::optional<int> translate_auto_never_count =
      dict->FindInt(kSourceLanguage);
  ASSERT_FALSE(translate_auto_never_count.has_value());

  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/true, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_TRUE(delegate->ShouldAutoNeverTranslate());

  std::optional<int> count =
      update_translate_denied_dict.FindInt(kSourceLanguage);
  EXPECT_EQ(std::optional<int>(0), count);
  // Get the dictionary again in order to update it.
  dict = &pref_service_->GetDict(TranslatePrefs::kPrefTranslateAutoNeverCount);
  translate_auto_never_count = dict->FindInt(kSourceLanguage);
  ASSERT_EQ(std::optional<int>(1), translate_auto_never_count);
}

TEST_F(TranslateInfoBarDelegateTest, ShouldAutoNeverTranslate_Not) {
  // Simulate being off-the-record.
  driver_.set_incognito();
  TranslateInfoBarDelegate::Create(
      /*replace_existing_infobar=*/false, manager_->GetWeakPtr(),
      infobar_manager_.get(), TranslateStep::TRANSLATE_STEP_TRANSLATING,
      kSourceLanguage, kTargetLanguage, TranslateErrors::NONE,
      /*triggered_from_menu=*/false);

  EXPECT_EQ(infobar_manager_->infobars().size(), 1u);
  TranslateInfoBarDelegate* delegate =
      infobar_manager_->infobars()[0]->delegate()->AsTranslateInfoBarDelegate();
  EXPECT_FALSE(delegate->ShouldAutoNeverTranslate());
}

}  // namespace translate

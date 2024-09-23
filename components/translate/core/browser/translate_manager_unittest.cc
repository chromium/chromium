// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_metrics_logger.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/translate_event.pb.h"

using testing::_;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;

namespace translate {
namespace {

const char kMenuTranslationIsAvailableName[] =
    "Translate.MenuTranslation.IsAvailable";

// Overrides NetworkChangeNotifier, simulating connection type changes
// for tests.
// TODO(groby): Combine with similar code in ResourceRequestAllowedNotifierTest.
class TestNetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : mock_notifier_(net::test::MockNetworkChangeNotifier::Create()) {}

  TestNetworkChangeNotifier(const TestNetworkChangeNotifier&) = delete;
  TestNetworkChangeNotifier& operator=(const TestNetworkChangeNotifier&) =
      delete;

  // Simulates a change of the connection type to |type|. This will notify any
  // objects that are NetworkChangeNotifiers.
  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    mock_notifier_->SetConnectionType(type);
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        type);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOffline() {
    mock_notifier_->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  void SimulateOnline() {
    mock_notifier_->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  }

 private:
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_notifier_;
};

// Compares TranslateEventProto on a restricted set of fields.
MATCHER_P(EqualsTranslateEventProto, translate_event, "") {
  const ::metrics::TranslateEventProto& tep(translate_event);
  return (arg.source_language() == tep.source_language() &&
          arg.target_language() == tep.target_language() &&
          arg.event_type() == tep.event_type());
}

// A language model that just returns its instance variable.
class MockLanguageModel : public language::LanguageModel {
 public:
  explicit MockLanguageModel(const std::vector<LanguageDetails>& in_details)
      : details(in_details) {}

  std::vector<LanguageDetails> GetLanguages() override { return details; }

  std::vector<LanguageDetails> details;
};

}  // namespace

namespace testing {

namespace metrics = TranslateBrowserMetrics;
using base::Bucket;
using ::testing::ElementsAre;

// The constructor of this class is used to register preferences before
// TranslatePrefs gets created.
struct ProfilePrefRegistration {
  explicit ProfilePrefRegistration(
      sync_preferences::TestingPrefServiceSyncable* prefs) {
    language::LanguagePrefs::RegisterProfilePrefs(prefs->registry());
    prefs->SetString(accept_languages_prefs, std::string());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    prefs->SetString(preferred_languages_prefs, std::string());
#endif
    TranslatePrefs::RegisterProfilePrefs(prefs->registry());
    // TODO(groby): Figure out RegisterProfilePrefs() should register this.
    prefs->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }
};

class TranslateManagerTest : public ::testing::Test {
 protected:
  TranslateManagerTest()
      : registration_(&prefs_),
        translate_prefs_(&prefs_),
        manager_(TranslateDownloadManager::GetInstance()),
        mock_translate_client_(&driver_, &prefs_),
        mock_language_model_({MockLanguageModel::LanguageDetails("en", 1.0)}) {}

  void SetUp() override {
    // Ensure we're not requesting a server-side translate language list.
    TranslateLanguageList::DisableUpdate();

    manager_->ResetForTesting();
  }

  void TearDown() override {
    manager_->ResetForTesting();
    variations::testing::ClearAllVariationParams();
  }

  // Utility function to prepare translate_manager_ for testing.
  void PrepareTranslateManager() {
    TranslateManager::SetIgnoreMissingKeyForTesting(true);
    translate_manager_ = std::make_unique<TranslateManager>(
        &mock_translate_client_, &mock_translate_ranker_,
        &mock_language_model_);
  }

  void SetHasLanguageChanged(bool has_language_changed) {
    translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
    translate_manager_->GetLanguageState()->DidNavigate(false, true, false,
                                                        std::string(), false);
    translate_manager_->GetLanguageState()->LanguageDetermined(
        has_language_changed ? "en" : "de", true);
    EXPECT_EQ(has_language_changed,
              translate_manager_->GetLanguageState()->HasLanguageChanged());
  }

  void InitTranslateEvent(const std::string& src_lang,
                          const std::string& dst_lang) {
    translate_manager_->InitTranslateEvent(src_lang, dst_lang,
                                           translate_prefs_);
  }

  NullTranslateMetricsLogger* null_translate_metrics_logger() {
    return translate_manager_->null_translate_metrics_logger_.get();
  }

  MockTranslateMetricsLogger* mock_translate_metrics_logger() {
    return mock_translate_metrics_logger_.get();
  }

  void ExpectHighestPriorityTriggerDecision(
      TriggerDecision highest_priority_trigger_decision) {
    mock_translate_metrics_logger_ =
        std::make_unique<MockTranslateMetricsLogger>();

    translate_manager_->RegisterTranslateMetricsLogger(
        mock_translate_metrics_logger_->GetWeakPtr());

    // Requires that the given highest priority trigger decision is logged with
    // the translate metrics logger first. After that value is logged, other
    // values can be logged.
    ::testing::Expectation highest_priority_trigger_decision_expectation =
        EXPECT_CALL(*mock_translate_metrics_logger_,
                    LogTriggerDecision(highest_priority_trigger_decision))
            .Times(1);
    EXPECT_CALL(*mock_translate_metrics_logger_, LogTriggerDecision(_))
        .Times(::testing::AnyNumber())
        .After(highest_priority_trigger_decision_expectation);
  }

  // Required to instantiate a net::test::MockNetworkChangeNotifier, because it
  // uses ObserverListThreadSafe.
  base::test::TaskEnvironment task_environment_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  sync_preferences::TestingPrefServiceSyncable prefs_;
  ProfilePrefRegistration registration_;
  // TODO(groby): request TranslatePrefs from |mock_translate_client_| instead.
  TranslatePrefs translate_prefs_;
  raw_ptr<TranslateDownloadManager> manager_;

  TestNetworkChangeNotifier network_notifier_;
  testing::MockTranslateDriver driver_;
  testing::MockTranslateRanker mock_translate_ranker_;
  ::testing::NiceMock<testing::MockTranslateClient> mock_translate_client_;
  MockLanguageModel mock_language_model_;
  std::unique_ptr<MockTranslateMetricsLogger> mock_translate_metrics_logger_;
  std::unique_ptr<TranslateManager> translate_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Target language comes from most recent target language if supported.
TEST_F(TranslateManagerTest, GetTargetLanguageFromRecentTargetLanguage) {
  // Set application language to Zulu for default.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("zu"));
  manager_->set_application_locale("zu");
  EXPECT_EQ("zu",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Check that a supported recent target language is used.
  translate_prefs_.SetRecentTargetLanguage("es");
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("es"));
  EXPECT_EQ("es",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Check that an auto translate language overrides the default recent target
  // language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  translate_prefs_.AddLanguagePairToAlwaysTranslateList("fr", "de");
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      nullptr, "fr"));

  // Check that a the default language is returned if the recent target is not
  // supported.
  translate_prefs_.SetRecentTargetLanguage("xx");
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xx"));
  EXPECT_EQ("zu",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));
}

// Target language comes from application locale if the locale's language
// is supported.
TEST_F(TranslateManagerTest, GetTargetLanguageDefaultsToAppLocale) {
  // Ensure the locale is set to a supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("es"));
  manager_->set_application_locale("es");
  EXPECT_EQ("es",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Try a second supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  manager_->set_application_locale("de");
  EXPECT_EQ("de",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Try a those case of non standard code.
  // 'he', 'fil', 'nb' => 'iw', 'tl', 'no'
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("iw"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("he"));
  manager_->set_application_locale("he");
  EXPECT_EQ("iw",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("tl"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("fil"));
  manager_->set_application_locale("fil");
  EXPECT_EQ("tl",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("no"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("nb"));
  manager_->set_application_locale("nb");
  EXPECT_EQ("no",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));

  // Use English if the application locale is not supported by translate.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xx"));
  manager_->set_application_locale("xx");
  EXPECT_EQ("en",
            TranslateManager::GetTargetLanguage(&translate_prefs_, nullptr));
}

// Test that the language model is used if provided.
TEST_F(TranslateManagerTest, GetTargetLanguageFromModel) {
  manager_->set_application_locale("ru");
  // Try with a single, supported language.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("es"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  // Try with two supported languages.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
      MockLanguageModel::LanguageDetails("es", 0.5)};
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  // Try with first supported language lower in the list.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xx"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("xx", 1.0),
      MockLanguageModel::LanguageDetails("es", 0.5)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  // Try with no supported languages.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("yy"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("xx", 1.0),
      MockLanguageModel::LanguageDetails("yy", 0.5)};
  // Should default to application locale.
  EXPECT_EQ("ru", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  // Try with no supported languages, unsupported app locale, and no accept
  // languages.
  manager_->set_application_locale("zz");
  // Should default to English.
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  // Try with no supported languages and unsupported app locale, but accept
  // languages.
  translate_prefs_.AddToLanguageList("de", /*force_blocked=*/false);
  // Should default to accept language.
  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  // Try non standard codes.
  // 'he', 'fil', 'nb' => 'iw', 'tl', 'no'
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("iw"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("he"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("he", 1.0)};
  EXPECT_EQ("iw", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("tl"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("fil"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("fil", 1.0)};
  EXPECT_EQ("tl", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));

  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("no"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("nb"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("nb", 1.0)};
  EXPECT_EQ("no", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));
}

TEST_F(TranslateManagerTest,
       OverrideTriggerWithIndiaEnglish_SourceUnspecified) {
  TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting();
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_,
          ShowTranslateUI(_, _, _, _, false /* triggered_from_menu */))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(1);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();
  EXPECT_EQ("en", TranslateManager::GetTargetLanguage(&translate_prefs_,
                                                      &mock_language_model_));
  translate_manager_->InitiateTranslation("en");
}

TEST_F(TranslateManagerTest, OverrideTriggerWithIndiaEnglish) {
  TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting();
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_,
          ShowTranslateUI(_, _, _, _, false /* triggered_from_menu */))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(1);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();
  EXPECT_EQ("hi", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, "en"));
  translate_manager_->InitiateTranslation("en");
}

TEST_F(TranslateManagerTest, OverrideTriggerWithIndiaEnglishReachThreshold) {
  TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting();
  manager_->set_application_locale("en");
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  // UI will be shown on consecutive calls to InitiateTranslation until the
  // threshold of four translations is reached.
  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowUI))
      .Times(3)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(4);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("en");
  // Initiate translation #2.
  translate_manager_->InitiateTranslation("en");
  // Initiate translation #3.
  translate_manager_->InitiateTranslation("en");
  // Initiate translation #4.
  translate_manager_->InitiateTranslation("en");

  // No other UI should be shown because the threshold has been reached.
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kDisabledSimilarLanguages));
  translate_manager_->InitiateTranslation("en");
}

TEST_F(TranslateManagerTest, OverrideTriggerWithIndiaEnglishAcceptPrompt) {
  TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting();
  manager_->set_application_locale("en");
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  // The UI should be shown once for each of the following three calls to
  // InitiateTranslation.
  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowUI))
      .Times(2)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(3);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("en");
  translate_manager_->TranslatePage("en", "hi", false);

  // Initiate translation again. The UI should be shown because Translation was
  // accepted by the user.
  translate_manager_->InitiateTranslation("en");

  // Initiating Translation again should still show the UI because accepting
  // once prevents backoff from occurring moving forward.
  translate_manager_->InitiateTranslation("en");
}

TEST_F(TranslateManagerTest, ShouldHonorRankerEnforcement_Enforce) {
  TranslatePrefs::SetShouldForceTriggerTranslateOnEnglishPagesForTesting();
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  // Simulate that Ranker decides to suppress the translation UI. This should be
  // honored since "enforce_ranker" is "true" in the experiment params.
  mock_translate_ranker_.set_should_offer_translation(false);

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  ExpectHighestPriorityTriggerDecision(TriggerDecision::kDisabledByRanker);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowUI))
      .Times(0);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();
  EXPECT_EQ("hi", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, "en"));
  translate_manager_->InitiateTranslation("en");
  EXPECT_TRUE(translate_manager_->GetLanguageState()->translate_enabled());
}

TEST_F(TranslateManagerTest, LanguageAddedToAcceptLanguagesAfterTranslation) {
  manager_->set_application_locale("en");
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(1);

  // Accept languages shouldn't contain "hi" before translating to that language
  std::vector<std::string> languages;
  mock_translate_client_.GetTranslatePrefs()->GetLanguageList(&languages);
  EXPECT_FALSE(base::Contains(languages, "hi"));

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("zu", true);
  network_notifier_.SimulateOnline();
  translate_manager_->InitiateTranslation("zu");

  translate_manager_->TranslatePage("zu", "hi", false);

  // Accept languages should now contain "hi" because the user chose to
  // translate to it once.
  mock_translate_client_.GetTranslatePrefs()->GetLanguageList(&languages);
  EXPECT_TRUE(base::Contains(languages, "hi"));
}

TEST_F(TranslateManagerTest,
       RedundantLanguageNotAddedToAcceptLanguagesAfterTranslation) {
  manager_->set_application_locale("en");
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(1);

  // Add a regional variant locale to the list of accepted languages.
  mock_translate_client_.GetTranslatePrefs()->AddToLanguageList("en-US", false);

  // Accept languages shouldn't contain "en" before translating to that language
  std::vector<std::string> languages;
  mock_translate_client_.GetTranslatePrefs()->GetLanguageList(&languages);
  EXPECT_FALSE(base::Contains(languages, "en"));

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();
  translate_manager_->InitiateTranslation("fr");

  EXPECT_FALSE(base::Contains(languages, "en"));
  translate_manager_->TranslatePage("fr", "en", false);

  // Accept languages should not contain "en" because it is redundant
  // with "en-US" already being present.
  languages.clear();
  mock_translate_client_.GetTranslatePrefs()->GetLanguageList(&languages);
  EXPECT_FALSE(base::Contains(languages, "en"));
}

TEST_F(TranslateManagerTest, DontTranslateOffline) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);

  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  // The test measures that the "Translate was disabled" exit can only be
  // reached after the early-out tests including IsOffline() passed.
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, false);

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);

  // In the offline case, Initiate won't trigger any translate behavior, so no
  // UI showing and no auto-translate.
  ExpectHighestPriorityTriggerDecision(TriggerDecision::kDisabledOffline);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowUI))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kAutomaticTranslationByPref))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kAutomaticTranslationByLink))
      .Times(0);

  network_notifier_.SimulateOffline();
  translate_manager_->InitiateTranslation("de");
}

TEST_F(TranslateManagerTest, TestRecordTranslateEvent) {
  PrepareTranslateManager();
  const std::string locale = "zh-TW";
  const std::string page_lang = "zh-CN";
  ::metrics::TranslateEventProto expected_tep;
  expected_tep.set_target_language(locale);
  expected_tep.set_source_language(page_lang);
  EXPECT_CALL(
      mock_translate_ranker_,
      RecordTranslateEvent(::metrics::TranslateEventProto::USER_ACCEPT, _,
                           Pointee(EqualsTranslateEventProto(expected_tep))))
      .Times(1);

  InitTranslateEvent(page_lang, locale);
  translate_manager_->RecordTranslateEvent(
      ::metrics::TranslateEventProto::USER_ACCEPT);
}

TEST_F(TranslateManagerTest,
       TestShouldOverrideMatchesPreviousLanguageDecision) {
  PrepareTranslateManager();
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideMatchesPreviousLanguageDecision(
          _,
          Pointee(EqualsTranslateEventProto(::metrics::TranslateEventProto()))))
      .WillOnce(Return(false));
  EXPECT_FALSE(
      translate_manager_->ShouldOverrideMatchesPreviousLanguageDecision());

  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideMatchesPreviousLanguageDecision(
          _,
          Pointee(EqualsTranslateEventProto(::metrics::TranslateEventProto()))))
      .WillOnce(Return(true));
  EXPECT_TRUE(
      translate_manager_->ShouldOverrideMatchesPreviousLanguageDecision());
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_Default) {
  PrepareTranslateManager();
  SetHasLanguageChanged(true);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI("fr"));
  histogram_tester.ExpectTotalCount(kTranslatePageLoadTriggerDecision, 0);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_HasLanguageChangedFalse) {
  PrepareTranslateManager();
  SetHasLanguageChanged(false);
  EXPECT_CALL(mock_translate_ranker_,
              ShouldOverrideMatchesPreviousLanguageDecision(_, _))
      .WillOnce(Return(false));

  ExpectHighestPriorityTriggerDecision(
      TriggerDecision::kDisabledMatchesPreviousLanguage);
  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI("fr"));
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_Override) {
  PrepareTranslateManager();
  base::HistogramTester histogram_tester;
  EXPECT_CALL(mock_translate_ranker_,
              ShouldOverrideMatchesPreviousLanguageDecision(_, _))
      .WillOnce(Return(true));
  SetHasLanguageChanged(false);
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI("fr"));
  histogram_tester.ExpectTotalCount(kTranslatePageLoadTriggerDecision, 0);
}

TEST_F(TranslateManagerTest,
       ShouldSuppressBubbleUI_HrefTranslateMatchesTarget) {
  PrepareTranslateManager();
  const std::string source_language = "de";
  const std::string target_language = "fr";

  // Set the LanguageState such that the language has not changed ("de" ->
  // "de"), but there is a hrefTranslate attribute that matches the target
  // language.
  translate_manager_->GetLanguageState()->LanguageDetermined(source_language,
                                                             true);
  translate_manager_->GetLanguageState()->DidNavigate(
      false, true, false,
      /*href_translate=*/target_language, false);
  translate_manager_->GetLanguageState()->LanguageDetermined(source_language,
                                                             true);
  EXPECT_FALSE(translate_manager_->GetLanguageState()->HasLanguageChanged());

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(target_language));
  histogram_tester.ExpectTotalCount(kTranslatePageLoadTriggerDecision, 0);
}

TEST_F(TranslateManagerTest,
       ShouldSuppressBubbleUI_HrefTranslateMismatchTarget) {
  PrepareTranslateManager();
  const std::string source_language = "de";
  const std::string target_language = "fr";

  // Set the LanguageState such that the language has not changed ("de" ->
  // "de"), and there is a hrefTranslate attribute that does not match the
  // target language.
  translate_manager_->GetLanguageState()->LanguageDetermined(source_language,
                                                             true);
  translate_manager_->GetLanguageState()->DidNavigate(false, true, false,
                                                      /*href_translate=*/"id",
                                                      false);
  translate_manager_->GetLanguageState()->LanguageDetermined(source_language,
                                                             true);
  EXPECT_FALSE(translate_manager_->GetLanguageState()->HasLanguageChanged());

  ExpectHighestPriorityTriggerDecision(
      TriggerDecision::kDisabledMatchesPreviousLanguage);
  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(target_language));
}

TEST_F(TranslateManagerTest, RecordInitilizationError) {
  PrepareTranslateManager();
  const std::string target_lang = "en";
  const std::string source_lang = "zh";
  ::metrics::TranslateEventProto expected_tep;
  expected_tep.set_target_language(target_lang);
  expected_tep.set_source_language(source_lang);
  EXPECT_CALL(
      mock_translate_ranker_,
      RecordTranslateEvent(::metrics::TranslateEventProto::INITIALIZATION_ERROR,
                           _, Pointee(EqualsTranslateEventProto(expected_tep))))
      .Times(1);

  InitTranslateEvent(source_lang, target_lang);
  translate_manager_->PageTranslated(source_lang, target_lang,
                                     TranslateErrors::INITIALIZATION_ERROR);
}

TEST_F(TranslateManagerTest, GetTargetLanguage_AutoTranslatedSource) {
  PrepareTranslateManager();

  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};

  translate_prefs_.AddLanguagePairToAlwaysTranslateList("fr", "de");

  // Add an unsupported language as target language.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xx"));
  translate_prefs_.AddLanguagePairToAlwaysTranslateList("af", "xx");

  EXPECT_EQ("de", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, "fr"));

  // Check that the model language is returned if the target language is not
  // supported.
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, "af"));

  // Check that the model language is returned for languages not on the always
  // translate list.
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, "zu"));

  // Also check that we're using the the source page lang code properly.
  translate_manager_->GetLanguageState()->LanguageDetermined("fr-CA", true);
  EXPECT_EQ("de", translate_manager_->GetTargetLanguageForDisplay(
                      &translate_prefs_, &mock_language_model_));

  EXPECT_FALSE(translate_manager_->GetLanguageState()->IsPageTranslated());
}

TEST_F(TranslateManagerTest, GetTargetLanguageForDisplay_NonTranslatedPage) {
  PrepareTranslateManager();
  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};

  EXPECT_EQ("es", translate_manager_->GetTargetLanguageForDisplay(
                      &translate_prefs_, &mock_language_model_));

  EXPECT_FALSE(translate_manager_->GetLanguageState()->IsPageTranslated());
}

TEST_F(TranslateManagerTest, GetTargetLanguageForDisplay_OnTranslatedPage) {
  PrepareTranslateManager();
  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  translate_manager_->GetLanguageState()->SetCurrentLanguage("de");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};

  EXPECT_EQ("de", translate_manager_->GetTargetLanguageForDisplay(
                      &translate_prefs_, &mock_language_model_));
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_PageNeedsTranslation) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  network_notifier_.SimulateOnline();

  base::HistogramTester histogram_tester;
  translate_manager_->GetLanguageState()->LanguageDetermined("de", false);
  // Users should be able to manually translate the page, even when
  // |page_level_translation_criteria_met| is false.
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
  histogram_tester.ExpectTotalCount(kMenuTranslationIsAvailableName, 0);
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate(true));
  histogram_tester.ExpectUniqueSample(kMenuTranslationIsAvailableName, true, 1);

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate(true));
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_Offline) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  base::HistogramTester histogram_tester;
  network_notifier_.SimulateOffline();
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());
  histogram_tester.ExpectTotalCount(kMenuTranslationIsAvailableName, 0);
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate(true));
  histogram_tester.ExpectUniqueSample(kMenuTranslationIsAvailableName, false,
                                      1);

  network_notifier_.SimulateOnline();
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate(true));
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_TranslatableURL) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  network_notifier_.SimulateOnline();

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(false));
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate(true));

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate(true));
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_EmptySourceLanguage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  network_notifier_.SimulateOnline();
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  translate_manager_->GetLanguageState()->LanguageDetermined("", true);

  // Manual translation when source language is empty is only supported on
  // Android.
  bool empty_source_supported = false;
#if BUILDFLAG(IS_ANDROID)
  empty_source_supported = true;
#endif
  EXPECT_EQ(translate_manager_->CanManuallyTranslate(), empty_source_supported);
  EXPECT_EQ(translate_manager_->CanManuallyTranslate(true),
            empty_source_supported);
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_UndefinedSourceLanguage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  network_notifier_.SimulateOnline();
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  translate_manager_->GetLanguageState()->LanguageDetermined(
      kUnknownLanguageCode, true);

  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
}

TEST_F(TranslateManagerTest, CanPartiallyTranslateTargetLanguage) {
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);
  translate_prefs_.SetRecentTargetLanguage("en");
  EXPECT_TRUE(translate_manager_->CanPartiallyTranslateTargetLanguage());

  translate_prefs_.SetRecentTargetLanguage("es");
  EXPECT_TRUE(translate_manager_->CanPartiallyTranslateTargetLanguage());

  translate_prefs_.SetRecentTargetLanguage("zh-CN");
  EXPECT_TRUE(translate_manager_->CanPartiallyTranslateTargetLanguage());

  translate_prefs_.SetRecentTargetLanguage("ilo");
  EXPECT_FALSE(translate_manager_->CanPartiallyTranslateTargetLanguage());

  translate_prefs_.SetRecentTargetLanguage("mni-Mtei");
  EXPECT_FALSE(translate_manager_->CanPartiallyTranslateTargetLanguage());
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage) {
  PrepareTranslateManager();
  manager_->set_application_locale("en");
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("zu"));

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));

  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage("ru");
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());

  translate_manager_->GetLanguageState()->LanguageDetermined("zu", true);

  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "zu", "ru",
                      TranslateErrors::NONE, /*triggered_from_menu=*/false))
      .WillOnce(Return(true));

  ExpectHighestPriorityTriggerDecision(TriggerDecision::kShowUI);
  translate_manager_->InitiateTranslation("zu");
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_ImagePage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  network_notifier_.SimulateOnline();
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  driver_.SetPageMimeType("image/png");

  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate(true));
}

TEST_F(TranslateManagerTest,
       PredefinedTargetLanguage_UserSpecifiedAutoTranslation) {
  PrepareTranslateManager();
  manager_->set_application_locale("en");

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));

  translate_prefs_.AddLanguagePairToAlwaysTranslateList("fr", "de");
  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage("ru", true);
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());

  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING, "fr", "de",
                      TranslateErrors::NONE, /*triggered_from_menu=*/false))
      .WillOnce(Return(true));

  ExpectHighestPriorityTriggerDecision(
      TriggerDecision::kAutomaticTranslationByPref);
  EXPECT_CALL(*mock_translate_metrics_logger_,
              LogTriggerDecision(TriggerDecision::kShowIcon))
      .Times(1);
  translate_manager_->InitiateTranslation("fr");
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage_BlockedLanguage) {
  PrepareTranslateManager();
  manager_->set_application_locale("en");

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  translate_prefs_.BlockLanguage("de");

  ASSERT_TRUE(translate_prefs_.IsBlockedLanguage("de"));
  ASSERT_FALSE(translate_prefs_.CanTranslateLanguage("de"));
  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage("ru");
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  EXPECT_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _)).Times(0);

  ExpectHighestPriorityTriggerDecision(
      TriggerDecision::kDisabledNeverTranslateLanguage);
  translate_manager_->InitiateTranslation("de");
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage_OverrideBlockedLanguage) {
  PrepareTranslateManager();
  manager_->set_application_locale("en");

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));

  translate_prefs_.BlockLanguage("de");

  ASSERT_TRUE(translate_prefs_.IsBlockedLanguage("de"));
  ASSERT_FALSE(translate_prefs_.CanTranslateLanguage("de"));
  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage(
      "ru", /*should_auto_translate=*/true);
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());
  EXPECT_TRUE(translate_manager_->GetLanguageState()
                  ->should_auto_translate_to_predefined_target_language());

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);

  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING, "de", "ru",
                      TranslateErrors::NONE, /*triggered_from_menu=*/false))
      .WillOnce(Return(true));

  TranslateMetricsLoggerImpl translate_metrics_logger(
      translate_manager_->GetWeakPtr());
  translate_metrics_logger.OnPageLoadStart(true);

  base::HistogramTester histogram_tester;
  translate_manager_->InitiateTranslation("de");

  translate_metrics_logger.RecordMetrics(true);
  EXPECT_THAT(histogram_tester.GetAllSamples(kTranslatePageLoadTriggerDecision),
              ElementsAre(Bucket(
                  static_cast<int>(
                      TriggerDecision::kAutomaticTranslationToPredefinedTarget),
                  1)));
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage_BlockedSite) {
  PrepareTranslateManager();
  manager_->set_application_locale("en");

  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));

  const GURL kTestUrl("https://www.example.com/");
  driver_.SetVisibleURL(kTestUrl);
  ON_CALL(mock_translate_client_, IsTranslatableURL(kTestUrl))
      .WillByDefault(Return(true));
  translate_prefs_.AddSiteToNeverPromptList(kTestUrl.HostNoBracketsPiece());
  ASSERT_TRUE(
      translate_prefs_.IsSiteOnNeverPromptList(kTestUrl.HostNoBracketsPiece()));

  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage("ru");
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  EXPECT_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _)).Times(0);

  ExpectHighestPriorityTriggerDecision(
      TriggerDecision::kDisabledNeverTranslateSite);
  translate_manager_->InitiateTranslation("de");
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage_AutoTranslate) {
  PrepareTranslateManager();
  manager_->set_application_locale("en");

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage(
      "ru", /*should_auto_translate=*/true);
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());
  EXPECT_TRUE(translate_manager_->GetLanguageState()
                  ->should_auto_translate_to_predefined_target_language());

  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);

  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING, "en", "ru",
                      TranslateErrors::NONE, /*triggered_from_menu=*/false))
      .WillOnce(Return(true));

  TranslateMetricsLoggerImpl translate_metrics_logger(
      translate_manager_->GetWeakPtr());
  translate_metrics_logger.OnPageLoadStart(true);

  base::HistogramTester histogram_tester;
  translate_manager_->InitiateTranslation("en");

  translate_metrics_logger.RecordMetrics(true);
  EXPECT_THAT(histogram_tester.GetAllSamples(kTranslatePageLoadTriggerDecision),
              ElementsAre(Bucket(
                  static_cast<int>(
                      TriggerDecision::kAutomaticTranslationToPredefinedTarget),
                  1)));

  // TODO(crbug.com/40743872): This test as well as many of the other
  // tests in this file should be verifying the state of the TranslateManager
  // after the translation happens, once the MockTranslateDriver is changed to
  // update the TranslateManager after a translation is performed.
}

TEST_F(TranslateManagerTest, ShowTranslateUI_NoTranslation) {
  manager_->set_application_locale("en");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));

  // TranslateManager::ShowTranslateUI should only call
  // TranslateClient::ShowTranslateUI (not do translation). If it also calls
  // TranslateManager::TranslatePage, ShowTranslateUI is called with a different
  // TranslateStep.
  EXPECT_CALL(mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_BEFORE_TRANSLATE, _, _, _,
                              false /* triggered_from_menu */))
      .WillOnce(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  network_notifier_.SimulateOnline();

  translate_manager_->ShowTranslateUI();
}

TEST_F(TranslateManagerTest, ShowTranslateUI_Translation) {
  manager_->set_application_locale("en");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
  };
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  // TranslateManager::ShowTranslateUI should result in a translation, reflected
  // by a call to TranslateClient::ShowTranslateUI using the
  // TRANSLATE_STEP_TRANSLATING step.
  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING, "en", "de",
                      TranslateErrors::NONE, false /* triggered_from_menu */))
      .WillOnce(Return(true));
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->ShowTranslateUI(/* auto_translate= */ true);
}

TEST_F(TranslateManagerTest, ShowTranslateUI_PageAlreadyTranslated) {
  manager_->set_application_locale("en");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("fr", 1.0),
  };
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  // TranslateManager::ShowTranslateUI should only call
  // TranslateClient::ShowTranslateUI (not do translation). When translation is
  // triggered ShowTranslateUI is called using the TRANSLATE_STEP_TRANSLATING
  // step.
  EXPECT_CALL(mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_AFTER_TRANSLATE, _, _, _,
                              false /* triggered_from_menu */))
      .WillOnce(Return(true));
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  translate_manager_->GetLanguageState()->SetCurrentLanguage("de");

  network_notifier_.SimulateOnline();

  translate_manager_->ShowTranslateUI(/* auto_translate= */ true);
}

TEST_F(TranslateManagerTest,
       ShowTranslateUI_ExplicitTargetLanguageTranslation) {
  manager_->set_application_locale("en");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  // TranslateManager::ShowTranslateUI should result in a translation, reflected
  // by a call to TranslateClient::ShowTranslateUI using the
  // TRANSLATE_STEP_TRANSLATING step and the specified languages.
  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING, "en", "pl",
                      TranslateErrors::NONE, false /* triggered_from_menu */))
      .WillOnce(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->ShowTranslateUI("pl", /* auto_translate */ true,
                                      /* triggered_from_menu= */ false);
}

TEST_F(TranslateManagerTest, ShowTranslateUI_ExplicitTargetSameAsTarget) {
  manager_->set_application_locale("en");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("pl", 1.0),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  // TranslateManager::ShowTranslateUI should result in a translation, reflected
  // by a call to TranslateClient::ShowTranslateUI using the
  // TRANSLATE_STEP_TRANSLATING step and the specified languages.
  EXPECT_CALL(
      mock_translate_client_,
      ShowTranslateUI(TRANSLATE_STEP_TRANSLATING, "de", "pl",
                      TranslateErrors::NONE, false /* triggered_from_menu */))
      .WillOnce(Return(true));

  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  network_notifier_.SimulateOnline();

  translate_manager_->ShowTranslateUI("pl", /* auto_translate */ true,
                                      /* triggered_from_menu= */ false);
}

TEST_F(TranslateManagerTest, GetActiveTranslateMetricsLogger) {
  PrepareTranslateManager();
  std::unique_ptr<TranslateMetricsLogger> translate_metrics_logger_a =
      std::make_unique<TranslateMetricsLoggerImpl>(
          translate_manager_->GetWeakPtr());
  std::unique_ptr<TranslateMetricsLogger> translate_metrics_logger_b =
      std::make_unique<TranslateMetricsLoggerImpl>(
          translate_manager_->GetWeakPtr());

  // Before either |TranslateMetricsLogger| begins, we expect
  // |GetActiveTranslateMetricsLogger| to return the null implementation.
  EXPECT_EQ(translate_manager_->GetActiveTranslateMetricsLogger(),
            null_translate_metrics_logger());

  // Now that the page load has begun for |translate_metrics_logger_a|, we
  // expect |GetActiveTranslateMetricsLogger| to return
  // |translate_metrics_logger_a|.
  translate_metrics_logger_a->OnPageLoadStart(true);
  EXPECT_EQ(translate_manager_->GetActiveTranslateMetricsLogger(),
            translate_metrics_logger_a.get());

  // Once the page load starts for |translate_metrics_logger_b|, we
  // expect |GetActiveTranslateMetricsLogger| to return
  // |translate_metrics_logger_b|, even if |translate_metrics_logger_a| hasn't
  // been destroyed yet.
  translate_metrics_logger_b->OnPageLoadStart(true);
  EXPECT_EQ(translate_manager_->GetActiveTranslateMetricsLogger(),
            translate_metrics_logger_b.get());

  // Once |translate_metrics_logger_b| is destroyed, we expect that
  // |GetActiveTranslateMetricsLogger| to return the null implementation.
  translate_metrics_logger_b.reset();
  EXPECT_EQ(translate_manager_->GetActiveTranslateMetricsLogger(),
            null_translate_metrics_logger());
}

TEST_F(TranslateManagerTest, HrefTranslateUnknownPageLanguage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  manager_->set_application_locale("en");
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  EXPECT_CALL(mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_TRANSLATING, "und", "ru",
                              TranslateErrors::NONE, false))
      .Times(1)
      .WillOnce(Return(true));
  network_notifier_.SimulateOnline();

  translate_manager_->GetLanguageState()->LanguageDetermined("und", true);
  translate_manager_->GetLanguageState()->DidNavigate(
      false, true, false,
      /*href_translate=*/"ru",
      /*navigation_from_google=*/true);
  translate_manager_->GetLanguageState()->LanguageDetermined("und", true);

  std::unique_ptr<TranslateMetricsLogger> translate_metrics_logger =
      std::make_unique<TranslateMetricsLoggerImpl>(
          translate_manager_->GetWeakPtr());
  translate_metrics_logger->OnPageLoadStart(true);

  base::HistogramTester histogram_tester;
  translate_manager_->InitiateTranslation("und");
  translate_metrics_logger->RecordMetrics(true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kTranslatePageLoadTriggerDecision),
      ElementsAre(Bucket(
          static_cast<int>(TriggerDecision::kAutomaticTranslationByHref), 1)));
}

TEST_F(TranslateManagerTest, HrefTranslateSimilarLanguages) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  manager_->set_application_locale("en");
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL()))
      .WillByDefault(Return(true));
  language::AcceptLanguagesService accept_languages(&prefs_,
                                                    accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetAcceptLanguagesService())
      .WillByDefault(Return(&accept_languages));
  EXPECT_CALL(mock_translate_client_,
              ShowTranslateUI(TRANSLATE_STEP_TRANSLATING, "und", "en",
                              TranslateErrors::NONE, false))
      .Times(1)
      .WillOnce(Return(true));
  network_notifier_.SimulateOnline();

  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  translate_manager_->GetLanguageState()->DidNavigate(
      false, true, false,
      /*href_translate=*/"en",
      /*navigation_from_google=*/true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);

  std::unique_ptr<TranslateMetricsLogger> translate_metrics_logger =
      std::make_unique<TranslateMetricsLoggerImpl>(
          translate_manager_->GetWeakPtr());
  translate_metrics_logger->OnPageLoadStart(true);

  base::HistogramTester histogram_tester;
  translate_manager_->InitiateTranslation("en");
  translate_metrics_logger->RecordMetrics(true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kTranslatePageLoadTriggerDecision),
      ElementsAre(Bucket(
          static_cast<int>(TriggerDecision::kAutomaticTranslationByHref), 1)));
}

}  // namespace testing

}  // namespace translate

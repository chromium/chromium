// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_constants.h"
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

const char kInitiationStatusName[] = "Translate.InitiationStatus.v2";

// Overrides NetworkChangeNotifier, simulating connection type changes
// for tests.
// TODO(groby): Combine with similar code in ResourceRequestAllowedNotifierTest.
class TestNetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : mock_notifier_(net::test::MockNetworkChangeNotifier::Create()) {}

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

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
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

namespace metrics = translate::TranslateBrowserMetrics;
using base::Bucket;
using ::testing::ElementsAre;

// The constructor of this class is used to register preferences before
// TranslatePrefs gets created.
struct ProfilePrefRegistration {
  ProfilePrefRegistration(sync_preferences::TestingPrefServiceSyncable* prefs) {
    language::LanguagePrefs::RegisterProfilePrefs(prefs->registry());
    prefs->SetString(accept_languages_prefs, std::string());
#if defined(OS_CHROMEOS)
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
        translate_prefs_(&prefs_,
                         accept_languages_prefs,
                         preferred_languages_prefs),
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
    translate_manager_ = std::make_unique<translate::TranslateManager>(
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

  void SetLanguageTooOftenDenied(const std::string& language) {
    translate_prefs_.UpdateLastDeniedTime(language);
    translate_prefs_.UpdateLastDeniedTime(language);

    EXPECT_TRUE(translate_prefs_.IsTooOftenDenied(language));
    EXPECT_FALSE(translate_prefs_.IsTooOftenDenied("other_language"));
  }

  void InitTranslateEvent(const std::string& src_lang,
                          const std::string& dst_lang) {
    translate_manager_->InitTranslateEvent(src_lang, dst_lang,
                                           translate_prefs_);
  }

  // Required to instantiate a net::test::MockNetworkChangeNotifier, because it
  // uses ObserverListThreadSafe.
  base::test::TaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  ProfilePrefRegistration registration_;
  // TODO(groby): request TranslatePrefs from |mock_translate_client_| instead.
  TranslatePrefs translate_prefs_;
  TranslateDownloadManager* manager_;

  TestNetworkChangeNotifier network_notifier_;
  translate::testing::MockTranslateDriver driver_;
  translate::testing::MockTranslateRanker mock_translate_ranker_;
  ::testing::NiceMock<translate::testing::MockTranslateClient>
      mock_translate_client_;
  MockLanguageModel mock_language_model_;
  std::unique_ptr<TranslateManager> translate_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

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

// Test that the language model is used if provided, and that languages that
// should be skipped actually are.
TEST_F(TranslateManagerTest, GetTargetLanguageFromModelWithSkippedLanguages) {
  // Try with a single, supported language but request it to be skipped. It
  // should still be chosen since there is no fallback.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("es"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"es"}));

  // Try with two supported languages and skip the first one.
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("de"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
      MockLanguageModel::LanguageDetails("es", 0.5)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"de"}));

  // Try with first supported language lower in the list but request it to be
  // skipped. It should still be chosen since there is no fallback.
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("xx"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("xx", 1.0),
      MockLanguageModel::LanguageDetails("es", 0.5)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"es"}));

  // Try non standard codes. Skipping should be specified using the supported
  // language in pairs of synonyms.
  // 'he', 'fil', 'nb' => 'iw', 'tl', 'no'
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("iw"));
  ASSERT_FALSE(TranslateDownloadManager::IsSupportedLanguage("he"));
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("he", 1.0),
      MockLanguageModel::LanguageDetails("es", 0.5)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"iw"}));

  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("iw", 1.0),
      MockLanguageModel::LanguageDetails("es", 0.5)};
  EXPECT_EQ("iw", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"he"}));
}

TEST_F(TranslateManagerTest, OverrideTriggerWithIndiaEnglishExperiment) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kOverrideTranslateTriggerInIndia,
      {{"override_model", "heuristic"}, {"enforce_ranker", "false"}});
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_languages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_,
          ShowTranslateUI(_, _, _, _, false /* triggered_from_menu */))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();
  EXPECT_EQ("hi", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"en"}));
  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));
}

TEST_F(TranslateManagerTest,
       OverrideTriggerWithIndiaEnglishExperimentThresholdAlreadyReached) {
  manager_->set_application_locale("en");
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kOverrideTranslateTriggerInIndia,
      {{"override_model", "heuristic"},
       {"enforce_ranker", "false"},
       {"backoff_threshold", "0"}});
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_,
          ShowTranslateUI(_, _, _, _, false /* triggered_from_menu */))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("en");
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName, metrics::INITIATION_STATUS_SIMILAR_LANGUAGES, 1);
}

TEST_F(TranslateManagerTest,
       OverrideTriggerWithIndiaEnglishExperimentReachingThreshold) {
  manager_->set_application_locale("en");
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kOverrideTranslateTriggerInIndia,
      {{"override_model", "heuristic"},
       {"enforce_ranker", "false"},
       {"backoff_threshold", "1"}});
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));

  // Initiate translation again. No other UI should be shown because the
  // threshold has been reached.
  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kInitiationStatusName),
      ElementsAre(Bucket(metrics::INITIATION_STATUS_SIMILAR_LANGUAGES, 1),
                  Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                  Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));
}

TEST_F(TranslateManagerTest,
       OverrideTriggerWithIndiaEnglishExperimentAcceptPrompt) {
  manager_->set_application_locale("en");
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kOverrideTranslateTriggerInIndia,
      {{"override_model", "heuristic"},
       {"enforce_ranker", "false"},
       {"backoff_threshold", "1"}});
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));

  translate_manager_->TranslatePage("en", "hi", false);

  // Initiate translation again. The UI should be shown because Translation was
  // accepted by the user.
  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 2),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 2)));

  // Initiating Translation again should still show the UI because accepting
  // once prevents backoff from occurring moving forward.
  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 3),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 3)));
}

TEST_F(TranslateManagerTest, ShouldHonorExperimentRankerEnforcement_Enforce) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kOverrideTranslateTriggerInIndia,
      {{"override_model", "heuristic"}, {"enforce_ranker", "true"}});
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("en", 1.0),
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  // Simulate that Ranker decides to suppress the translation UI. This should be
  // honored since "enforce_ranker" is "true" in the experiment params.
  mock_translate_ranker_.set_should_offer_translation(false);

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();
  EXPECT_EQ("hi", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {"en"}));
  translate_manager_->InitiateTranslation("en");
  EXPECT_TRUE(translate_manager_->GetLanguageState()->translate_enabled());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kInitiationStatusName),
      ElementsAre(Bucket(metrics::INITIATION_STATUS_ABORTED_BY_RANKER, 1),
                  Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1),
                  Bucket(metrics::INITIATION_STATUS_SUPPRESS_INFOBAR, 1)));
}

TEST_F(TranslateManagerTest,
       ShouldHonorExperimentRankerEnforcement_DontEnforce) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kOverrideTranslateTriggerInIndia,
      {{"override_model", "heuristic"}, {"enforce_ranker", "false"}});
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("hi", 1.),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  // Simulate that Ranker decides to suppress the translation UI. This should
  // not be honored since "enforce_ranker" is "true" in the experiment params.
  mock_translate_ranker_.set_should_offer_translation(false);

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  network_notifier_.SimulateOnline();
  EXPECT_EQ("hi", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {}));
  translate_manager_->InitiateTranslation("fr");
  EXPECT_TRUE(translate_manager_->GetLanguageState()->translate_enabled());
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));
}

TEST_F(TranslateManagerTest, LanguageAddedToAcceptLanguagesAfterTranslation) {
  manager_->set_application_locale("en");
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("hi", 0.5),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  // Accept languages shouldn't contain "hi" before translating to that language
  std::vector<std::string> languages;
  mock_translate_client_.GetTranslatePrefs()->GetLanguageList(&languages);
  EXPECT_FALSE(base::Contains(languages, "hi"));

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));

  translate_manager_->TranslatePage("en", "hi", false);

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
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  // Add a regional variant locale to the list of accepted languages.
  mock_translate_client_.GetTranslatePrefs()->AddToLanguageList("en-US", false);

  // Accept languages shouldn't contain "en" before translating to that language
  std::vector<std::string> languages;
  mock_translate_client_.GetTranslatePrefs()->GetLanguageList(&languages);
  EXPECT_FALSE(base::Contains(languages, "en"));

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateTranslation("fr");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ElementsAre(Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1),
                          Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));

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

  TranslateAcceptLanguages accept_languages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_languages));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  // The test measures that the "Translate was disabled" exit can only be
  // reached after the early-out tests including IsOffline() passed.
  base::HistogramTester histogram_tester;

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, false);

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);

  // In the offline case, Initiate won't trigger any translate behavior, so no
  // UI showing and no auto-translate.
  network_notifier_.SimulateOffline();
  translate_manager_->InitiateTranslation("de");
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ::testing::Not(::testing::Contains(
                  Bucket(metrics::INITIATION_STATUS_SHOW_INFOBAR, 1))));
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ::testing::Not(::testing::Contains(
                  Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1))));
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ::testing::Not(::testing::Contains(
                  Bucket(metrics::INITIATION_STATUS_AUTO_BY_CONFIG, 1))));
  EXPECT_THAT(histogram_tester.GetAllSamples(kInitiationStatusName),
              ::testing::Not(::testing::Contains(
                  Bucket(metrics::INITIATION_STATUS_AUTO_BY_LINK, 1))));
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

TEST_F(TranslateManagerTest, TestShouldOverrideDecision) {
  PrepareTranslateManager();
  const int kEventType = 1;
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          kEventType, _,
          Pointee(EqualsTranslateEventProto(::metrics::TranslateEventProto()))))
      .WillOnce(Return(false));
  EXPECT_FALSE(translate_manager_->ShouldOverrideDecision(kEventType));

  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          kEventType, _,
          Pointee(EqualsTranslateEventProto(::metrics::TranslateEventProto()))))
      .WillOnce(Return(true));
  EXPECT_TRUE(translate_manager_->ShouldOverrideDecision(kEventType));
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_Default) {
  PrepareTranslateManager();
  SetHasLanguageChanged(true);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(true, "en"));
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_HasLanguageChangedFalse) {
  PrepareTranslateManager();
  SetHasLanguageChanged(false);
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          ::metrics::TranslateEventProto::MATCHES_PREVIOUS_LANGUAGE, _, _))
      .WillOnce(Return(false));
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      metrics::INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE, 1);

  EXPECT_CALL(mock_translate_ranker_, ShouldOverrideDecision(_, _, _))
      .WillOnce(Return(false));

  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(true, "en"));
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      metrics::INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE, 2);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_IsTooOftenDenied) {
  PrepareTranslateManager();
  SetHasLanguageChanged(true);
  SetLanguageTooOftenDenied("en");
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          ::metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_AUTO_BLACKLIST,
          _, _))
      .WillOnce(Return(false));
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "de"));
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(true, "en"));
  histogram_tester.ExpectUniqueSample(
      kInitiationStatusName,
      metrics::INITIATION_STATUS_ABORTED_BY_TOO_OFTEN_DENIED, 1);
}

TEST_F(TranslateManagerTest, ShouldSuppressBubbleUI_Override) {
  PrepareTranslateManager();
  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          ::metrics::TranslateEventProto::MATCHES_PREVIOUS_LANGUAGE, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      mock_translate_ranker_,
      ShouldOverrideDecision(
          ::metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_AUTO_BLACKLIST,
          _, _))
      .WillOnce(Return(true));
  SetHasLanguageChanged(false);
  SetLanguageTooOftenDenied("en");
  EXPECT_FALSE(translate_manager_->ShouldSuppressBubbleUI(false, "en"));
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);
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

TEST_F(TranslateManagerTest, GetManualSourceAndTargetLanguages) {
  PrepareTranslateManager();
  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  EXPECT_EQ("fr", translate_manager_->GetLanguageState()->original_language());
  EXPECT_EQ("fr", translate_manager_->GetLanguageState()->current_language());
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {}));

  EXPECT_FALSE(translate_manager_->GetLanguageState()->IsPageTranslated());

  const std::string source_code = TranslateDownloadManager::GetLanguageCode(
      translate_manager_->GetLanguageState()->original_language());
  EXPECT_EQ("fr", source_code);

  const std::string target_lang = TranslateManager::GetManualTargetLanguage(
      source_code, *translate_manager_->GetLanguageState(), &translate_prefs_,
      &mock_language_model_);
  EXPECT_EQ("es", target_lang);
}

TEST_F(TranslateManagerTest,
       GetManualSourceAndTargetLanguages_OnTranslatedPage) {
  PrepareTranslateManager();
  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  translate_manager_->GetLanguageState()->SetCurrentLanguage("de");
  EXPECT_EQ("fr", translate_manager_->GetLanguageState()->original_language());
  EXPECT_EQ("de", translate_manager_->GetLanguageState()->current_language());
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("es", 1.0)};
  EXPECT_EQ("es", TranslateManager::GetTargetLanguage(
                      &translate_prefs_, &mock_language_model_, {}));

  EXPECT_TRUE(translate_manager_->GetLanguageState()->IsPageTranslated());

  const std::string source_code = TranslateDownloadManager::GetLanguageCode(
      translate_manager_->GetLanguageState()->original_language());
  EXPECT_EQ("fr", source_code);

  const std::string target_lang = TranslateManager::GetManualTargetLanguage(
      source_code, *translate_manager_->GetLanguageState(), &translate_prefs_,
      &mock_language_model_);
  EXPECT_EQ("de", target_lang);
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_PageNeedsTranslation) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  network_notifier_.SimulateOnline();

  translate_manager_->GetLanguageState()->LanguageDetermined("de", false);
  // Users should be able to manually translate the page, even when
  // |page_needs_translation| is false.
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_Offline) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));

  network_notifier_.SimulateOffline();
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());

  network_notifier_.SimulateOnline();
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_TranslatableURL) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  network_notifier_.SimulateOnline();

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(false));
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_EmptySourceLanguage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  network_notifier_.SimulateOnline();
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));

  translate_manager_->GetLanguageState()->LanguageDetermined("", true);

  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());
}

TEST_F(TranslateManagerTest, CanManuallyTranslate_UndefinedSourceLanguage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  network_notifier_.SimulateOnline();
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));

  translate_manager_->GetLanguageState()->LanguageDetermined(
      translate::kUnknownLanguageCode, true);

  // Translation of unknown source language pages is supported on desktop
  // platforms, but not mobile.
#if defined(OS_ANDROID) || defined(OS_IOS)
  EXPECT_FALSE(translate_manager_->CanManuallyTranslate());
#else
  EXPECT_TRUE(translate_manager_->CanManuallyTranslate());
#endif
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  manager_->set_application_locale("en");
  ASSERT_TRUE(TranslateDownloadManager::IsSupportedLanguage("en"));

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));
  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage("ru");
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());

  translate_manager_->GetLanguageState()->LanguageDetermined("en", true);

  base::HistogramTester histogram_tester;
  translate_manager_->InitiateTranslation("en");
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kInitiationStatusName),
      ElementsAre(Bucket(
          metrics::INITIATION_STATUS_SHOW_UI_PREDEFINED_TARGET_LANGUAGE, 1)));
}

TEST_F(TranslateManagerTest, PredefinedTargetLanguage_HonourUserSettings) {
  TranslateManager::SetIgnoreMissingKeyForTesting(true);
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);
  manager_->set_application_locale("en");

  // Add german language to the list of accepted languages.
  // If a lang is not in the list than CanTranslateLanguage(|lang|) returns true
  // even if a language is blocked.
  prefs_.registry()->RegisterStringPref("accept_languages.test", "de");
  TranslateAcceptLanguages accept_languages(&prefs_, "accept_languages.test");

  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_languages));
  ON_CALL(mock_translate_client_, ShowTranslateUI(_, _, _, _, _))
      .WillByDefault(Return(true));

  translate_prefs_.BlockLanguage("de");
  translate_prefs_.WhitelistLanguagePair("fr", "de");

  ASSERT_TRUE(translate_prefs_.IsBlockedLanguage("de"));
  ASSERT_FALSE(translate_prefs_.CanTranslateLanguage(&accept_languages, "de"));

  network_notifier_.SimulateOnline();

  translate_manager_->SetPredefinedTargetLanguage("ru");
  EXPECT_EQ(
      "ru",
      translate_manager_->GetLanguageState()->GetPredefinedTargetLanguage());

  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);

  base::HistogramTester histogram_tester;
  translate_manager_->InitiateTranslation("de");
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kInitiationStatusName),
      ElementsAre(Bucket(metrics::INITIATION_STATUS_DISABLED_BY_CONFIG, 1)));

  translate_manager_->GetLanguageState()->LanguageDetermined("fr", true);
  translate_manager_->InitiateTranslation("fr");
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kInitiationStatusName),
      ElementsAre(Bucket(metrics::INITIATION_STATUS_DISABLED_BY_CONFIG, 1),
                  Bucket(metrics::INITIATION_STATUS_AUTO_BY_CONFIG, 1),
                  Bucket(metrics::INITIATION_STATUS_SHOW_ICON, 1)));
}

TEST_F(TranslateManagerTest, InitiateManualTranslation) {
  manager_->set_application_locale("en");
  mock_language_model_.details = {
      MockLanguageModel::LanguageDetails("de", 1.0),
  };
  ON_CALL(mock_translate_client_, IsTranslatableURL(GURL::EmptyGURL()))
      .WillByDefault(Return(true));
  TranslateAcceptLanguages accept_langugages(&prefs_, accept_languages_prefs);
  ON_CALL(mock_translate_client_, GetTranslateAcceptLanguages())
      .WillByDefault(Return(&accept_langugages));
  EXPECT_CALL(mock_translate_client_,
              ShowTranslateUI(_, _, _, _, true /* triggered_from_menu */))
      .WillOnce(Return(true));

  translate_manager_ = std::make_unique<translate::TranslateManager>(
      &mock_translate_client_, &mock_translate_ranker_, &mock_language_model_);

  base::HistogramTester histogram_tester;
  prefs_.SetBoolean(prefs::kOfferTranslateEnabled, true);
  translate_manager_->GetLanguageState()->LanguageDetermined("de", true);
  network_notifier_.SimulateOnline();

  translate_manager_->InitiateManualTranslation();

  // InitiateManualTranslation should only ShowTranslateUI (not do translation).
  histogram_tester.ExpectTotalCount(kInitiationStatusName, 0);
}

}  // namespace testing

}  // namespace translate

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

namespace {

const char kFirstSpeechResult[] = "the brown fox";
const char kSecondSpeechResult[] = "the brown fox jumped over the lazy dog";

const char kEnglishUS[] = "en-US";

constexpr char kS3FallbackReasonMetricName[] =
    "Ash.Projector.OnDeviceToServerSpeechRecognitionFallbackReason";

inline void SetLocale(const std::string& locale) {
  g_browser_process->SetApplicationLocale(locale);
}

// A mocked version instance of SodaInstaller for testing purposes.
class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller() = default;
  MockSodaInstaller(const MockSodaInstaller&) = delete;
  MockSodaInstaller& operator=(const MockSodaInstaller&) = delete;
  ~MockSodaInstaller() override = default;

  MOCK_CONST_METHOD0(GetSodaBinaryPath, base::FilePath());
  MOCK_CONST_METHOD1(GetLanguagePath, base::FilePath(const std::string&));
  MOCK_METHOD2(InstallLanguage, void(const std::string&, PrefService*));
  MOCK_METHOD2(UninstallLanguage, void(const std::string&, PrefService*));
  MOCK_CONST_METHOD0(GetAvailableLanguages, std::vector<std::string>());
  MOCK_METHOD1(InstallSoda, void(PrefService*));
  MOCK_METHOD1(UninstallSoda, void(PrefService*));
};

class MockLocaleUpdateController : public ash::LocaleUpdateController {
 public:
  MockLocaleUpdateController() = default;
  MockLocaleUpdateController(const MockLocaleUpdateController&) = delete;
  MockLocaleUpdateController& operator=(const MockLocaleUpdateController&) =
      delete;
  ~MockLocaleUpdateController() override = default;

  MOCK_METHOD0(OnLocaleChanged, void());
  MOCK_METHOD4(ConfirmLocaleChange,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    LocaleChangeConfirmationCallback));
  MOCK_METHOD1(AddObserver, void(ash::LocaleChangeObserver*));
  MOCK_METHOD1(RemoveObserver, void(ash::LocaleChangeObserver*));
};

struct ProjectorClientTestScenario {
  const std::vector<base::test::FeatureRef> enabled_features;
  const std::vector<base::test::FeatureRef> disabled_features;

  ProjectorClientTestScenario(
      const std::vector<base::test::FeatureRef>& enabled,
      const std::vector<base::test::FeatureRef>& disabled)
      : enabled_features(enabled), disabled_features(disabled) {}
};

}  // namespace

class ProjectorClientImplUnitTest
    : public testing::TestWithParam<ProjectorClientTestScenario> {
 public:
  ProjectorClientImplUnitTest() = default;

  ProjectorClientImplUnitTest(const ProjectorClientImplUnitTest&) = delete;
  ProjectorClientImplUnitTest& operator=(const ProjectorClientImplUnitTest&) =
      delete;
  ~ProjectorClientImplUnitTest() override = default;

  Profile* profile() { return testing_profile_; }

  MockProjectorController& projector_controller() {
    return projector_controller_;
  }

  ProjectorClient* client() { return projector_client_.get(); }

  void SetUp() override {
    const auto& parameter = GetParam();
    scoped_feature_list_.InitWithFeatures(parameter.enabled_features,
                                          parameter.disabled_features);

    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ = ProfileManager::GetPrimaryUserProfile();
    ASSERT_TRUE(testing_profile_);

    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&ProjectorClientImplUnitTest::
                                    CreateTestSpeechRecognitionService,
                                base::Unretained(this)));
    SetLocale(kEnglishUS);
    soda_installer_ = std::make_unique<MockSodaInstaller>();
    ON_CALL(*soda_installer_, GetAvailableLanguages)
        .WillByDefault(testing::Return(std::vector<std::string>({kEnglishUS})));
    soda_installer_->NotifySodaInstalledForTesting();
    soda_installer_->NotifySodaInstalledForTesting(speech::LanguageCode::kEnUs);
    mock_app_client_ = std::make_unique<MockAppClient>();
    mock_locale_controller_ = std::make_unique<MockLocaleUpdateController>();
    projector_client_ =
        std::make_unique<ProjectorClientImpl>(&projector_controller_);
  }

  void TearDown() override {
    projector_client_.reset();
    mock_locale_controller_.reset();
    mock_app_client_.reset();
    soda_installer_.reset();
    testing::Test::TearDown();
  }

  std::unique_ptr<KeyedService> CreateTestSpeechRecognitionService(
      content::BrowserContext* context) {
    std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
        std::make_unique<speech::FakeSpeechRecognitionService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  void SendSpeechResult(const char* result, bool is_final) {
    EXPECT_TRUE(fake_service_->is_capturing_audio());
    base::RunLoop loop;
    fake_service_->SendSpeechRecognitionResult(
        media::SpeechRecognitionResult(result, is_final));
    loop.RunUntilIdle();
  }

  void SendTranscriptionError() {
    EXPECT_TRUE(fake_service_->is_capturing_audio());
    base::RunLoop loop;
    fake_service_->SendSpeechRecognitionError();
    loop.RunUntilIdle();
  }

 protected:
  // Start speech recognition and verify on device to server based speech
  // recognition fallback reason. If `expected_reason` is null, there will be no
  // metric recorded because there is no fallback to server based recognition.
  void MaybeReportToServerBasedFallbackReasonMetricAndVerify(
      absl::optional<ash::OnDeviceToServerSpeechRecognitionFallbackReason>
          expected_reason,
      int expected_count) {
    ProjectorClientImpl* client =
        static_cast<ProjectorClientImpl*>(projector_client_.get());
    client->StartSpeechRecognition();
    if (expected_reason.has_value()) {
      histogram_tester_.ExpectBucketCount(kS3FallbackReasonMetricName,
                                          /*sample=*/expected_reason.value(),
                                          /*expected_count=*/expected_count);
    } else {
      EXPECT_TRUE(client->GetSpeechRecognitionAvailability().use_on_device);
    }

    client->OnSpeechRecognitionStopped();
  }

  base::HistogramTester histogram_tester_;

  content::BrowserTaskEnvironment task_environment_;
  Profile* testing_profile_ = nullptr;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  MockProjectorController projector_controller_;
  std::unique_ptr<ProjectorClient> projector_client_;
  std::unique_ptr<MockSodaInstaller> soda_installer_;
  std::unique_ptr<MockAppClient> mock_app_client_;
  std::unique_ptr<MockLocaleUpdateController> mock_locale_controller_;
  speech::FakeSpeechRecognitionService* fake_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ProjectorClientImplUnitTest, SpeechRecognitionResults) {
  client()->StartSpeechRecognition();
  fake_service_->WaitForRecognitionStarted();

  EXPECT_CALL(projector_controller(),
              OnTranscription(
                  media::SpeechRecognitionResult(kFirstSpeechResult, false)));
  SendSpeechResult(kFirstSpeechResult, /*is_final=*/false);

  EXPECT_CALL(projector_controller(),
              OnTranscription(
                  media::SpeechRecognitionResult(kSecondSpeechResult, false)));
  SendSpeechResult(kSecondSpeechResult, /*is_final=*/false);

  EXPECT_CALL(projector_controller(), OnTranscriptionError());
  SendTranscriptionError();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

const char kArabic[] = "ar";
const char kFrench[] = "fr";
const char kChinese[] = "zh-TW";
const char kUnsupportedLanguage[] = "am";
const char kSpanishLatam[] = "es-419";
const char kSpanishMexican[] = "es-mx";
const char kEnglishNewZealand[] = "en-nz";

bool IsEqualAvailability(const SpeechRecognitionAvailability& first,
                         const SpeechRecognitionAvailability& second) {
  if (first.use_on_device != second.use_on_device)
    return false;

  if (first.use_on_device)
    return first.on_device_availability == second.on_device_availability;

  return first.server_based_availability == second.server_based_availability;
}

}  // namespace

TEST_P(ProjectorClientImplUnitTest, SpeechRecognitionAvailability) {
  const bool force_enable_server_based =
      features::ShouldForceEnableServerSideSpeechRecognitionForDev();
  const bool server_based_available =
      features::IsInternalServerSideSpeechRecognitionEnabled();

  SetLocale(kFrench);

  SpeechRecognitionAvailability availability;
  availability.use_on_device = false;
  availability.server_based_availability =
      ash::ServerBasedRecognitionAvailability::kAvailable;
  if (server_based_available) {
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));

    SetLocale(kArabic);
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));

    SetLocale(kChinese);
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));

    SetLocale(kSpanishLatam);
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));

    SetLocale(kSpanishMexican);
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));

    SetLocale(kEnglishNewZealand);
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));
  } else {
    availability.use_on_device = true;
    availability.on_device_availability =
        ash::OnDeviceRecognitionAvailability::kUserLanguageNotAvailable;
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));
  }

  SetLocale(kEnglishUS);
  if (force_enable_server_based && server_based_available) {
    availability.use_on_device = false;
    availability.server_based_availability =
        ash::ServerBasedRecognitionAvailability::kAvailable;
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));
  } else {
    availability.use_on_device = true;
    availability.on_device_availability =
        ash::OnDeviceRecognitionAvailability::kAvailable;
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));
  }

  SetLocale(kUnsupportedLanguage);

  if (force_enable_server_based) {
    availability.use_on_device = false;
    availability.server_based_availability =
        ash::ServerBasedRecognitionAvailability::kUserLanguageNotAvailable;
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));
  } else {
    availability.use_on_device = true;
    availability.on_device_availability =
        ash::OnDeviceRecognitionAvailability::kUserLanguageNotAvailable;
    SetLocale(kUnsupportedLanguage);
    EXPECT_TRUE(IsEqualAvailability(
        projector_client_->GetSpeechRecognitionAvailability(), availability));
  }
}

TEST_P(ProjectorClientImplUnitTest, FallbackReasonMetric) {
  const bool force_enable_server_based =
      features::ShouldForceEnableServerSideSpeechRecognitionForDev();
  const bool server_based_available =
      features::IsInternalServerSideSpeechRecognitionEnabled();

  size_t expect_total_count = 0;

  SetLocale(kFrench);
  if (server_based_available) {
    if (force_enable_server_based) {
      MaybeReportToServerBasedFallbackReasonMetricAndVerify(
          ash::OnDeviceToServerSpeechRecognitionFallbackReason::kEnforcedByFlag,
          1);
      expect_total_count++;
    } else {
      // French is not available for SODA:
      MaybeReportToServerBasedFallbackReasonMetricAndVerify(
          ash::OnDeviceToServerSpeechRecognitionFallbackReason::
              kUserLanguageNotAvailableForSoda,
          1);

      // Set available language for SODA:
      SetLocale(kEnglishUS);

      // Verify metric reports for SODA error:
      speech::SodaInstaller::GetInstance()->UninstallSodaForTesting();

      speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
          speech::LanguageCode::kEnUs,
          speech::SodaInstaller::ErrorCode::kUnspecifiedError);
      MaybeReportToServerBasedFallbackReasonMetricAndVerify(
          ash::OnDeviceToServerSpeechRecognitionFallbackReason::
              kSodaInstallationErrorUnspecified,
          1);
      speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
          speech::LanguageCode::kEnUs,
          speech::SodaInstaller::ErrorCode::kNeedsReboot);
      MaybeReportToServerBasedFallbackReasonMetricAndVerify(
          ash::OnDeviceToServerSpeechRecognitionFallbackReason::
              kSodaInstallationErrorNeedsReboot,
          1);
      expect_total_count += 3;
    }
  } else {
    if (!force_enable_server_based) {
      // Set available language for SODA:
      SetLocale(kEnglishUS);

      // No metric reports if SODA is available:
      MaybeReportToServerBasedFallbackReasonMetricAndVerify(absl::nullopt, 0);
    }
  }

  histogram_tester_.ExpectTotalCount(kS3FallbackReasonMetricName,
                                     /*count=*/expect_total_count);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

INSTANTIATE_TEST_SUITE_P(
    ProjectorClientTestScenarios,
    ProjectorClientImplUnitTest,
    ::testing::Values(
        ProjectorClientTestScenario({features::kProjector,
                                     features::kOnDeviceSpeechRecognition},
                                    {}),
        ProjectorClientTestScenario(
            {features::kProjector, features::kOnDeviceSpeechRecognition,
             features::kForceEnableServerSideSpeechRecognitionForDev},
            {}),
        ProjectorClientTestScenario(
            {features::kProjector,
             features::kInternalServerSideSpeechRecognition,
             features::kOnDeviceSpeechRecognition},
            {features::kForceEnableServerSideSpeechRecognitionForDev})));

}  // namespace ash

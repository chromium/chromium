// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_manager.h"

#include <memory>
#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "chromeos/ash/components/growth/mock_campaigns_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace growth {
namespace {

inline constexpr char kValidCampaignsFileTemplate[] = R"(
    {
      "0": [
        // List is an invalid targeting.
        {
          "id": 1,
          "targetings": [
            []
          ],
          "payload": {}
        },
        // String is an invalid campaign.
        "Invalid campaign",
        {
          "id": 3,
          "studyId":1,
          "targetings": [
            {
              %s
            }
          ],
          "payload": {
            "demoModeApp": {
              "attractionLoop": {
                "videoSrcLang1": "/asset/peripherals_lang1.mp4",
                "videoSrcLang2": "/asset/peripherals_lang2.mp4"
              }
            }
          }
        }
      ]
    }
)";

inline constexpr char kValidCampaignsFileMultiTargetingsTemplate[] = R"(
    {
      "0": [
        // List is an invalid targeting.
        {
          "id": 1,
          "targetings": [
            []
          ],
          "payload": {}
        },
        // String is an invalid campaign.
        "Invalid campaign",
        {
          "id": 3,
          "studyId":1,
          "targetings": %s,
          "payload": {
            "demoModeApp": {
              "attractionLoop": {
                "videoSrcLang1": "/asset/peripherals_lang1.mp4",
                "videoSrcLang2": "/asset/peripherals_lang2.mp4"
              }
            }
          }
        }
      ]
    }
)";

inline constexpr char kValidDemoModeTargeting[] = R"(
    "demoMode": {
      "retailers": ["bby", "bestbuy", "bbt"],
      "storeIds": ["2", "4", "6"],
      "countries": ["US"],
      "capability": {
        "isCloudGamingDevice": true,
        "isFeatureAwareDevice": true
      }
    }
)";

inline constexpr char kValidMultiTargetings[] = R"([
    // Targeting 1.
    {
      "runtime": {
        "appsOpened": [{"appId": "app_id_1"}]
      }
    },
    // Targeting 2.
    {
      "runtime": {
        "appsOpened": [{"appId": "app_id_2"}]
      }
    },
    // Targeting 3.
    {
      "runtime": {
        "appsOpened": [{"appId": "app_id_3"}]
      }
    }
  ])";

inline constexpr char kCampaignsFileName[] = "campaigns.json";

inline constexpr char kCampaignsExperimentTag[] = "exp_tag";

inline constexpr char kCampaignsManagerErrorHistogramName[] =
    "Ash.Growth.CampaignsManager.Error";

inline constexpr char
    kCampaignsComponentDownloadDurationSessionStartHistogram[] =
        "Ash.Growth.CampaignsComponent.DownloadDurationSessionStart";

inline constexpr char kCampaignsComponentDownloadDurationOobeHistogram[] =
    "Ash.Growth.CampaignsComponent.DownloadDurationInOobe";

inline constexpr char kCampaignsComponentReadDurationHistogram[] =
    "Ash.Growth.CampaignsComponent.ParseDuration";

inline constexpr char kCampaignMatchDurationHistogram[] =
    "Ash.Growth.CampaignsManager.MatchDuration";

inline constexpr char kGetCampaignBySlotHistogramName[] =
    "Ash.Growth.CampaignsManager.GetCampaignBySlot";

// testing::InvokeArgument<N> does not work with base::OnceCallback. Use this
// gmock action template to invoke base::OnceCallback. `k` is the k-th argument
// and `T` is the callback's type.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(const_cast<T&>(std::get<k>(args))).Run(p0);
}

}  // namespace

class TestCampaignsManagerObserver : public CampaignsManager::Observer {
 public:
  // Spins a RunLoop until campaigns are loaded.
  void Wait() { run_loop_.Run(); }

  void OnCampaignsLoadCompleted() override {
    load_completed_ = true;
    run_loop_.Quit();
  }

  bool load_completed() { return load_completed_; }

 private:
  base::RunLoop run_loop_;
  bool load_completed_ = false;
};

class CampaignsManagerTest : public testing::Test {
 public:
  CampaignsManagerTest() = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    InitializePrefService();

    campaigns_manager_ =
        std::make_unique<CampaignsManager>(&mock_client_, local_state_.get());
    campaigns_manager_->SetPrefs(pref_.get());
  }

 protected:
  void LoadComponentAndVerifyLoadComplete(std::string_view file_content,
                                          bool in_oobe = false) {
    TestCampaignsManagerObserver observer;
    campaigns_manager_->AddObserver(&observer);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath campaigns_file(
        temp_dir_.GetPath().Append(kCampaignsFileName));

    base::WriteFile(campaigns_file, file_content);

    EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
        .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
            temp_dir_.GetPath()));

    base::test::TestFuture<void> load_completed_waiter;
    campaigns_manager_->LoadCampaigns(load_completed_waiter.GetCallback(),
                                      in_oobe);
    ASSERT_TRUE(load_completed_waiter.Wait());
    observer.Wait();

    ASSERT_TRUE(observer.load_completed());
  }

  void MockDemoMode(bool in_demo_mode,
                    bool cloud_gaming_device,
                    bool feature_aware_device,
                    std::string_view store_id,
                    std::string_view retailer_id,
                    std::string_view country) {
    EXPECT_CALL(mock_client_, IsDeviceInDemoMode)
        .WillRepeatedly(testing::Return(in_demo_mode));
    EXPECT_CALL(mock_client_, IsCloudGamingDevice)
        .WillRepeatedly(testing::Return(cloud_gaming_device));
    EXPECT_CALL(mock_client_, IsFeatureAwareDevice)
        .WillRepeatedly(testing::Return(feature_aware_device));
    local_state_->SetString(ash::prefs::kDemoModeStoreId, store_id);
    local_state_->SetString(ash::prefs::kDemoModeRetailerId, retailer_id);
    local_state_->SetString(ash::prefs::kDemoModeCountry, country);
  }

  void MockDemoMode(bool in_demo_mode,
                    bool cloud_gaming_device,
                    bool feature_aware_device,
                    std::string_view store_id,
                    std::string_view retailer_id,
                    std::string_view country,
                    const base::Version& app_version) {
    MockDemoMode(in_demo_mode, cloud_gaming_device, feature_aware_device,
                 store_id, retailer_id, country);
    EXPECT_CALL(mock_client_, GetDemoModeAppVersion)
        .WillRepeatedly(testing::ReturnRef(app_version));
  }

  void InitilizeCampaignsExperimentTag(const std::string& exp_tag) {
    base::FieldTrialParams params;
    params[kCampaignsExperimentTag] = exp_tag;
    base::test::FeatureRefAndParams campaigns_experiment_tag(
        ash::features::kGrowthCampaignsExperimentTagTargeting, params);

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {campaigns_experiment_tag}, {});
  }

  void VerifyDemoModePayload(const Campaign* campaign) {
    const auto* payload = campaign->FindDictByDottedPath("payload.demoModeApp");
    ASSERT_EQ("/asset/peripherals_lang1.mp4",
              *payload->FindStringByDottedPath("attractionLoop.videoSrcLang1"));
    ASSERT_EQ("/asset/peripherals_lang2.mp4",
              *payload->FindStringByDottedPath("attractionLoop.videoSrcLang2"));
  }

  void LoadComponentWithBasicDeviceTargetings(
      const std::string& milestone_range,
      std::optional<bool> target_feature_aware_device = std::nullopt) {
    std::string feature_aware_targeting = "";
    if (target_feature_aware_device) {
      std::string isFeatureAwareDeviceStr =
          target_feature_aware_device.value() ? "true" : "false";
      feature_aware_targeting =
          base::StringPrintf(R"(,
        "isFeatureAwareDevice": %s
      )",
                             isFeatureAwareDeviceStr.c_str());
    }
    auto device_targeting = base::StringPrintf(R"(
            "device": {
              "locales": ["en-US"],
              "milestone": {
                %s
              }%s
            }
          )",
                                               milestone_range.c_str(),
                                               feature_aware_targeting.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, device_targeting.c_str()));
  }

  void LoadComponentWithRegisteredTimeTargeting(
      const std::string& registerd_time_targeting) {
    auto device_targeting =
        base::StringPrintf(R"(
            "device": {
              "registeredTime": %s
            }
          )",
                           registerd_time_targeting.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, device_targeting.c_str()));
  }

  void LoadComponentWithDeviceTargeting(const std::string& device_targeting) {
    auto targeting = base::StringPrintf(R"(
            "device": %s
          )",
                                        device_targeting.c_str());
    LoadComponentAndVerifyLoadComplete(
        base::StringPrintf(kValidCampaignsFileTemplate, targeting.c_str()));
  }

  void LoadComponentWithSessionTargeting(
      const std::string& session_targeting_str) {
    auto session_targeting = base::StringPrintf(R"(
            "session": %s
          )",
                                                session_targeting_str.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, session_targeting.c_str()));
  }

  void LoadComponentWithExperimentTagTargeting(const std::string& exp_tags) {
    auto session_targeting = base::StringPrintf(R"(
            "session": {
              "experimentTags": %s
            }
          )",
                                                exp_tags.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, session_targeting.c_str()));
  }

  void LoadComponentWithScheduling(const std::string& schedulings) {
    auto session_targeting = base::StringPrintf(R"(
            "runtime": {
              "schedulings": %s
            }
          )",
                                                schedulings.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, session_targeting.c_str()));
  }

  void LoadComponentWithTriggerTargeting(const std::string& triggers) {
    auto session_targeting = base::StringPrintf(R"(
            "runtime": {
              "triggers": %s
            }
          )",
                                                triggers.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, session_targeting.c_str()));
  }

  void LoadComponentWithAppsOpenedTargeting(const std::string& apps_opened) {
    auto session_targeting = base::StringPrintf(R"(
            "runtime": {
              "appsOpened": %s
            }
          )",
                                                apps_opened.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, session_targeting.c_str()));
  }

  void LoadComponentWithMultiTargetings(const std::string& targetings) {
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileMultiTargetingsTemplate, targetings.c_str()));
  }

  void LoadComponentWithActiveUrlTargeting(const std::string& active_url) {
    auto session_targeting = base::StringPrintf(R"(
            "runtime": {
              "activeUrlRegexes": %s
            }
          )",
                                                active_url.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, session_targeting.c_str()));
  }

  base::test::TaskEnvironment task_environment_;
  MockCampaignsManagerClient mock_client_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
  std::unique_ptr<CampaignsManager> campaigns_manager_;
  // A sub-class might override this from `InitializeScopedFeatureList`.
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  void InitializePrefService() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    pref_ = std::make_unique<TestingPrefServiceSimple>();

    local_state_->registry()->RegisterStringPref(ash::prefs::kDemoModeCountry,
                                                 "US");
    local_state_->registry()->RegisterStringPref(
        ash::prefs::kDemoModeRetailerId, std::string());
    local_state_->registry()->RegisterStringPref(ash::prefs::kDemoModeStoreId,
                                                 std::string());
  }
};

TEST_F(CampaignsManagerTest, LoadAndGetDemoModeCampaign) {
  base::HistogramTester histogram_tester;
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationSessionStartHistogram, 1);
  histogram_tester.ExpectTotalCount(kCampaignsComponentReadDurationHistogram,
                                    1);
  histogram_tester.ExpectTotalCount(kCampaignMatchDurationHistogram, 0);

  EXPECT_CALL(mock_client_,
              RegisterSyntheticFieldTrial(std::optional<int>(1), 3));
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));

  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidTargeting,
                                     /*count=*/1);

  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidCampaign,
                                     /*count=*/1);
  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationSessionStartHistogram, 1);
  histogram_tester.ExpectTotalCount(kCampaignsComponentReadDurationHistogram,
                                    1);
  histogram_tester.ExpectTotalCount(kCampaignMatchDurationHistogram, 1);

  histogram_tester.ExpectUniqueSample(kGetCampaignBySlotHistogramName,
                                      Slot::kDemoModeApp,
                                      /*expected_bucket_count=*/1);
}

TEST_F(CampaignsManagerTest, LoadAndGetDemoModeCampaignInOobe) {
  base::HistogramTester histogram_tester;

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting),
      /*in_oobe=*/true);

  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationSessionStartHistogram, 0);

  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationOobeHistogram, 1);
}

TEST_F(CampaignsManagerTest, GetCampaignNoTargeting) {
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, ""));

  EXPECT_CALL(mock_client_,
              RegisterSyntheticFieldTrial(std::optional<int>(1), 3));
  // Verify that the campaign is selected if there is no demo mode targeting.
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignNoTargetingNotInDemoMode) {
  MockDemoMode(
      /*in_demo_mode=*/false,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, ""));

  EXPECT_CALL(mock_client_,
              RegisterSyntheticFieldTrial(std::optional<int>(1), 3));
  // Verify that the campaign is selected if there is not in demo mode.
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

// TODO(b/302360652): After user prefs targeting is implemented, add test to
// verify that campaign with user prefs related targeting is not selected when
// user prefs are not available.

TEST_F(CampaignsManagerTest, GetDemoModeCampaignNotInDemoMode) {
  base::HistogramTester histogram_tester;

  MockDemoMode(
      /*in_demo_mode=*/false,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
  histogram_tester.ExpectUniqueSample(kGetCampaignBySlotHistogramName,
                                      Slot::kDemoModeApp,
                                      /*expected_bucket_count=*/0);
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignNotGamingDevice) {
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/false,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignNotFeatureAwareDevice) {
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/false,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignstoreIdMismatch) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"1",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignRetailerIdMismatch) {
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"abc",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignCanonicalizedRetailerId) {
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bestbuy",
      /*country=*/"US");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate,
                         R"(
          "demoMode": {
            "retailers": ["best-buy", "best_buy"],
            "storeIds": ["2", "4", "6"],
            "countries": ["US"],
            "capability": {
              "isCloudGamingDevice": true,
              "isFeatureAwareDevice": true
            }
          }
      )"));

  // Verify that the campaign is selected if there is not in demo mode.
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignCountryMismatch) {
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"UK");

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionTargeting) {
  const base::Version expected_app_version("1.0.0.0");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.0",
        "max": "1.0.0.1"
      }
    }
)"));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMinMismatch) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.1",
        "max": "1.0.0.2"
      }
    }
  )"));

  const base::Version expected_app_version("1.0.0.0");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMaxMismatch) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.0",
        "max": "1.0.0.1"
      }
    }
  )"));

  const base::Version expected_app_version("1.0.0.2");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMinOnly) {
  const base::Version expected_app_version("1.0.0.3");

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.0"
      }
    }
  )"));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMinOnlyMismstch) {
  const base::Version expected_app_version("1.0.0.2");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.3",
      }
    }
  )"));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMaxOnly) {
  const base::Version expected_app_version("1.0.0.3");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "max": "1.0.0.3"
      }
    }
  )"));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMaxOnlyMismstch) {
  const base::Version expected_app_version("1.0.0.4");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "max": "1.0.0.3"
      }
    }
  )"));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionInvalidAppVersion) {
  const base::Version expected_app_version = base::Version();
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "max": "1.0.0.3"
      }
    }
  )"));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, LoadCampaignsFailed) {
  base::HistogramTester histogram_tester;
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          std::nullopt));

  campaigns_manager_->LoadCampaigns(base::DoNothing());
  observer.Wait();
  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationSessionStartHistogram, 1);
  histogram_tester.ExpectTotalCount(kCampaignsComponentReadDurationHistogram,
                                    0);

  ASSERT_TRUE(observer.load_completed());

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));

  histogram_tester.ExpectBucketCount(
      kCampaignsManagerErrorHistogramName,
      CampaignsManagerError::kCampaignsComponentLoadFail,
      /*count=*/1);
  histogram_tester.ExpectTotalCount(kCampaignMatchDurationHistogram, 1);
}

TEST_F(CampaignsManagerTest, LoadCampaignsNoFile) {
  base::HistogramTester histogram_tester;
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          temp_dir_.GetPath()));

  campaigns_manager_->LoadCampaigns(base::DoNothing());
  observer.Wait();
  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationSessionStartHistogram, 1);
  histogram_tester.ExpectTotalCount(kCampaignsComponentReadDurationHistogram,
                                    1);

  ASSERT_TRUE(observer.load_completed());

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));

  histogram_tester.ExpectBucketCount(
      kCampaignsManagerErrorHistogramName,
      CampaignsManagerError::kCampaignsFileLoadFail,
      /*count=*/1);
  histogram_tester.ExpectTotalCount(kCampaignMatchDurationHistogram, 1);
}

TEST_F(CampaignsManagerTest, LoadCampaignsInvalidFile) {
  base::HistogramTester histogram_tester;

  LoadComponentAndVerifyLoadComplete("abc");
  histogram_tester.ExpectTotalCount(
      kCampaignsComponentDownloadDurationSessionStartHistogram, 1);
  histogram_tester.ExpectTotalCount(kCampaignsComponentReadDurationHistogram,
                                    1);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));

  histogram_tester.ExpectBucketCount(
      kCampaignsManagerErrorHistogramName,
      CampaignsManagerError::kCampaignsParsingFail,
      /*count=*/1);
  histogram_tester.ExpectTotalCount(kCampaignMatchDurationHistogram, 1);
}

TEST_F(CampaignsManagerTest, LoadCampaignsEmptyFile) {
  LoadComponentAndVerifyLoadComplete("");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignDeviceTargeting) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(base::StringPrintf(
      R"(
      "min": %d,
      "max": %d
    )",
      current_version, current_version + 1));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMilestoneMinMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(base::StringPrintf(
      R"(
      "min": %d,
      "max": %d
    )",
      current_version + 1, current_version + 1));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMilestoneMaxMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(base::StringPrintf(
      R"(
        "min": %d,
        "max": %d
      )",
      current_version - 2, current_version - 1));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMinMilestoneOnly) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("min": %d)", current_version));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMinMilestoneOnlyMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("min": %d)", current_version + 1));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMaxMilestoneOnly) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMaxMilestoneOnlyMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version - 1));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignApplicationLocaleMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-CA")));
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetFeatureAwareDevice) {
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  scoped_feature_list_.InitWithFeatures(
      {ash::features::kFeatureManagementGrowthFramework}, {});

  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version),
      /*target_feature_aware_device=*/true);

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetFeatureAwareDeviceMismatch) {
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kFeatureManagementGrowthFramework}, {});

  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version),
      /*target_feature_aware_device=*/false);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetNotFeatureAwareDevice) {
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  scoped_feature_list_.InitWithFeatures(
      {}, {ash::features::kFeatureManagementGrowthFramework});

  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version),
      /*target_feature_aware_device=*/false);

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetNotFeatureAwareDeviceMismatch) {
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));
  scoped_feature_list_.InitWithFeatures(
      {}, {ash::features::kFeatureManagementGrowthFramework});

  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithBasicDeviceTargetings(
      base::StringPrintf(R"("max": %d)", current_version),
      /*target_feature_aware_device=*/true);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetDeviceAge) {
  campaigns_manager_->SetOobeCompleteTimeForTesting(base::Time::Now() -
                                                    base::Hours(26281));
  // 3 years to 4 years.
  LoadComponentWithDeviceTargeting(base::StringPrintf(R"({
        "deviceAgeInHours": {
          "start": 26280,
          "end": 35040
        }
      })"));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetDeviceAgeStartOnly) {
  campaigns_manager_->SetOobeCompleteTimeForTesting(base::Time::Now() -
                                                    base::Hours(26281));
  LoadComponentWithDeviceTargeting(base::StringPrintf(R"({
        "deviceAgeInHours": {
          "start": 26280
        }
      })"));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetDeviceAgeEndOnly) {
  campaigns_manager_->SetOobeCompleteTimeForTesting(base::Time::Now() -
                                                    base::Hours(35039));
  LoadComponentWithDeviceTargeting(base::StringPrintf(R"({
        "deviceAgeInHours": {
          "end": 35040
        }
      })"));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetDeviceAgeMismatch) {
  campaigns_manager_->SetOobeCompleteTimeForTesting(base::Time::Now() -
                                                    base::Hours(26279));
  LoadComponentWithDeviceTargeting(base::StringPrintf(R"({
        "deviceAgeInHours": {
          "start": 26280,
          "end": 35040
        }
      })"));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetDeviceAgeStartOnlyMismatch) {
  campaigns_manager_->SetOobeCompleteTimeForTesting(base::Time::Now() -
                                                    base::Hours(26279));
  LoadComponentWithDeviceTargeting(base::StringPrintf(R"({
        "deviceAgeInHours": {
          "start": 26280
        }
      })"));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTargetDeviceAgeEndOnlyMismatch) {
  campaigns_manager_->SetOobeCompleteTimeForTesting(base::Time::Now() -
                                                    base::Hours(35041));
  LoadComponentWithDeviceTargeting(base::StringPrintf(R"({
        "deviceAgeInHours": {
          "end": 35040
        }
      })"));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignExperimentTag) {
  InitilizeCampaignsExperimentTag(/*exp_tag=*/"1");

  LoadComponentWithExperimentTagTargeting(R"(["1", "2", "3"])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignExperimentTagOrRelationship) {
  InitilizeCampaignsExperimentTag(/*exp_tag=*/"2");

  LoadComponentWithExperimentTagTargeting(R"(["1", "2", "3"])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignExperimentTagMismatch) {
  InitilizeCampaignsExperimentTag(/*exp_tag=*/"4");

  LoadComponentWithExperimentTagTargeting(R"(["1", "2", "3"])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignForOwner) {
  LoadComponentWithSessionTargeting(R"({
    "isOwner": true
  })");

  campaigns_manager_->SetIsUserOwner(true);

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignForOwnerMismatch) {
  LoadComponentWithSessionTargeting(R"({
    "isOwner": true
  })");

  campaigns_manager_->SetIsUserOwner(false);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignForNonOwner) {
  LoadComponentWithSessionTargeting(R"({
    "isOwner": false
  })");

  campaigns_manager_->SetIsUserOwner(false);

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignForNonOwnerMismatch) {
  LoadComponentWithSessionTargeting(R"({
    "isOwner": false
  })");

  campaigns_manager_->SetIsUserOwner(true);

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaign) {
  const auto now = base::Time::Now();
  auto start = now;
  auto end = now + base::Seconds(5);
  LoadComponentWithScheduling(base::StringPrintf(
      R"([{"start": %f, "end": %f}])", start.InSecondsFSinceUnixEpoch(),
      end.InSecondsFSinceUnixEpoch()));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignMultipleSchedulings) {
  const auto now = base::Time::Now();
  // First scheduling start and end before now.
  auto start = now - base::Seconds(10);
  auto end = now - base::Seconds(5);

  // Second scheduling start after now.
  auto start2 = now + base::Seconds(10);
  auto end2 = now + base::Seconds(20);

  // Third scheduling start now and end 10 secs from now.
  auto start3 = now;
  auto end3 = now + base::Seconds(10);
  LoadComponentWithScheduling(base::StringPrintf(
      R"([
          {"start": %f, "end": %f},
          {"start": %f, "end": %f},
          {"start": %f, "end": %f}
        ])",
      start.InSecondsFSinceUnixEpoch(), end.InSecondsFSinceUnixEpoch(),
      start2.InSecondsFSinceUnixEpoch(), end2.InSecondsFSinceUnixEpoch(),
      start3.InSecondsFSinceUnixEpoch(), end3.InSecondsFSinceUnixEpoch()));

  // Verify that there is a match.
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignMismatch) {
  const auto now = base::Time::Now();
  auto start = now + base::Seconds(5);
  auto end = now + base::Seconds(10);
  LoadComponentWithScheduling(base::StringPrintf(
      R"([{"start": %f, "end": %f}])", start.InSecondsFSinceUnixEpoch(),
      end.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignStartOnly) {
  const auto now = base::Time::Now();
  LoadComponentWithScheduling(
      base::StringPrintf(R"([{"start": %f}])", now.InSecondsFSinceUnixEpoch()));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignStartOnlyMismatch) {
  const auto now = base::Time::Now();
  auto start = now + base::Seconds(5);
  LoadComponentWithScheduling(base::StringPrintf(
      R"([{"start": %f}])", start.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignEndOnly) {
  const auto now = base::Time::Now();
  auto end = now + base::Seconds(5);
  LoadComponentWithScheduling(
      base::StringPrintf(R"([{"end": %f}])", end.InSecondsFSinceUnixEpoch()));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignEndOnlyMismatch) {
  const auto now = base::Time::Now();
  auto end = now - base::Seconds(10);
  LoadComponentWithScheduling(
      base::StringPrintf(R"([{"end": %f}])", end.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignInvalidTargeting) {
  base::HistogramTester histogram_tester;

  LoadComponentWithScheduling("1");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));

  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidCampaign,
                                     /*count=*/1);

  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidTargeting,
                                     /*count=*/1);
}

TEST_F(CampaignsManagerTest, GetSchedulingCampaignInvalidScheduling) {
  base::HistogramTester histogram_tester;

  LoadComponentWithScheduling(R"([
    "test1",
    "test2"
  ])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));

  // Verify that two of the scheduling is invalids.
  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidScheduling,
                                     /*count=*/2);

  // There is a invalid campaign in the list of campaigns.
  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidCampaign,
                                     /*count=*/1);

  // There is a campaign with invalid targeting in the list of campaigns.
  histogram_tester.ExpectBucketCount(kCampaignsManagerErrorHistogramName,
                                     CampaignsManagerError::kInvalidTargeting,
                                     /*count=*/1);
}

TEST_F(CampaignsManagerTest, GetCampaignWithRegisteredTimeTargeting) {
  const auto now = base::Time::Now();
  auto start = now;
  auto end = now + base::Seconds(5);
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(base::StringPrintf(
      R"({"start": %f, "end": %f})", start.InSecondsFSinceUnixEpoch(),
      end.InSecondsFSinceUnixEpoch()));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignWithRegisteredTimeTargetingStartOnly) {
  const auto now = base::Time::Now();
  auto start = now;
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(
      base::StringPrintf(R"({"start": %f})", start.InSecondsFSinceUnixEpoch()));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignWithRegisteredTimeTargetingEndOnly) {
  const auto now = base::Time::Now();
  auto end = now + base::Seconds(5);
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(
      base::StringPrintf(R"({"end": %f})", end.InSecondsFSinceUnixEpoch()));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest,
       GetCampaignWithRegisteredTimeTargetingStartMismatch) {
  const auto now = base::Time::Now();
  auto start = now + base::Seconds(5);
  auto end = now + base::Seconds(10);
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(base::StringPrintf(
      R"({"start": %f, "end": %f})", start.InSecondsFSinceUnixEpoch(),
      end.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest,
       GetCampaignWithRegisteredTimeTargetingEndMismatch) {
  const auto now = base::Time::Now();
  auto start = now - base::Seconds(2);
  auto end = now - base::Seconds(1);
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(base::StringPrintf(
      R"({"start": %f, "end": %f})", start.InSecondsFSinceUnixEpoch(),
      end.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest,
       GetCampaignWithRegisteredTimeTargetingStartOnlyMismatch) {
  const auto now = base::Time::Now();
  auto start = now + base::Seconds(5);
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(
      base::StringPrintf(R"({"start": %f})", start.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest,
       GetCampaignWithRegisteredTimeTargetingEndOnlyMismatch) {
  const auto now = base::Time::Now();
  auto end = now - base::Seconds(5);
  campaigns_manager_->SetOobeCompleteTimeForTesting(now);
  LoadComponentWithRegisteredTimeTargeting(
      base::StringPrintf(R"({"end": %f})", end.InSecondsFSinceUnixEpoch()));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignActiveUrl) {
  campaigns_manager_->SetActiveUrl(GURL("https://www.google.com/?foo=bar"));

  LoadComponentWithActiveUrlTargeting(
      R"([
        "https://www\\.google\\.com/\\?foo=bar",
        "https://gmail\\.google\\.com/\\?foo=bar",
        "https://www\\.google\\.com/\\?foo=bar2"
    ])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignActiveUrlOrRelationship) {
  campaigns_manager_->SetActiveUrl(GURL("https://www.google.com/?foo=bar"));

  LoadComponentWithActiveUrlTargeting(
      R"([
        "https://gmail\\.google\\.com/\\?foo=bar",
        "https://www\\.google\\.com/\\?foo=bar",
        "https://www\\.google\\.com/\\?foo=bar2",
        "https://www\\.google\\.com/foo=bar"
    ])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignActiveUrlMismatch) {
  campaigns_manager_->SetActiveUrl(GURL("https://www.google.com/?foo=bar"));

  LoadComponentWithActiveUrlTargeting(
      R"([
        "1https://gmail\\.google\\.com/\\?foo=bar",
        "http://www\\.google\\.com/\\?foo=bar",
        "https://www\\.google\\.com/\\?foo=bar2"
    ])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignActiveUrlEmptyActiveUrl) {
  campaigns_manager_->SetActiveUrl(GURL::EmptyGURL());

  LoadComponentWithActiveUrlTargeting(
      R"([
        "1https://gmail\\.google\\.com/\\?foo=bar",
        "http://www\\.google\\.com/\\?foo=bar",
        "https://www\\.google\\.com/\\?foo=bar2"
    ])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignActiveUrlNoActiveUrl) {
  LoadComponentWithActiveUrlTargeting(
      R"([
        "1https://gmail\\.google\\.com/\\?foo=bar",
        "http://www\\.google\\.com/\\?foo=bar",
        "https://www\\.google\\.com/\\?foo=bar2"
    ])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTriggers) {
  campaigns_manager_->SetTrigger(TriggeringType::kAppOpened);

  LoadComponentWithTriggerTargeting(R"([0])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTriggersOrRelationship) {
  campaigns_manager_->SetTrigger(TriggeringType::kAppOpened);

  LoadComponentWithTriggerTargeting(R"([0, 1])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTriggersMissmatch) {
  campaigns_manager_->SetTrigger(TriggeringType::kAppOpened);

  LoadComponentWithTriggerTargeting(R"([1])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignTriggersNoTrigger) {
  LoadComponentWithTriggerTargeting(R"([0, 1])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignAppsOpened) {
  campaigns_manager_->SetOpenedApp("app_id_1");

  LoadComponentWithAppsOpenedTargeting(
      R"([
      {"appId": "app_id_1"},
      {"appId": "app_id_10"},
      {"appId": "app_id_15"}
    ])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignAppsOpenedOrRelationship) {
  campaigns_manager_->SetOpenedApp("app_id_10");

  LoadComponentWithAppsOpenedTargeting(
      R"([
      {"appId": "app_id_1"},
      {"appId": "app_id_10"},
      {"appId": "app_id_15"}
    ])");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignAppOpenedMismatch) {
  campaigns_manager_->SetOpenedApp("app_id_2");

  LoadComponentWithAppsOpenedTargeting(
      R"([
      {"appId": "app_id_1"},
      {"appId": "app_id_10"},
      {"appId": "app_id_15"}
    ])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignAppOpenedNoOpenedApp) {
  LoadComponentWithAppsOpenedTargeting(
      R"([
      {"appId": "app_id_1"},
      {"appId": "app_id_10"},
      {"appId": "app_id_15"}
    ])");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMatchFirstTargeting) {
  campaigns_manager_->SetOpenedApp("app_id_1");

  LoadComponentWithMultiTargetings(kValidMultiTargetings);
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMatchSecondTargeting) {
  campaigns_manager_->SetOpenedApp("app_id_2");

  LoadComponentWithMultiTargetings(kValidMultiTargetings);
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMatchMultiTargetingsMismatch) {
  campaigns_manager_->SetOpenedApp("app_id_20");

  LoadComponentWithMultiTargetings(kValidMultiTargetings);
  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, CampaignsFilteringTest) {
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  LoadComponentAndVerifyLoadComplete(
      R"({
          "0": [
            {
              "id": 3,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-GB"]
                  }
                }
              ],
              "payload": {}
            },
            {
              "id": 3,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-AU"]
                  }
                }
              ],
              "payload": {}
            },
            {
              "id": 3,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-US"]
                  }
                }
              ],
              "payload": {}
            }
          ],
          "3": [
            {
              "id": 4,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-GB"]
                  }
                }
              ],
              "payload": {}
            },
            {
              "id": 4,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-AU"]
                  }
                }
              ],
              "payload": {}
            },
            {
              "id": 4,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-US"]
                  }
                }
              ],
              "payload": {}
            },
            {
              "id": 5,
              "targetings": [
                {
                  "device": {
                    "locales": ["en-US"]
                  },
                  "runtime": {
                    "appsOpened": [
                      {
                        "appId": "foo"
                      }
                    ]
                  }
                }
              ],
              "payload": {}
            }
          ]
        }
      )",
      /*in_oobe=*/false);

  // Verify that prematch is ran to filter campaigns at campaign load time.
  ASSERT_EQ(1u,
            campaigns_manager_->GetCampaignsBySlotForTesting(Slot::kDemoModeApp)
                ->size());

  // Verify that runtime targeting is not used for prematch. The campaign with
  // runtime targeting is not filtered.
  ASSERT_EQ(
      2u, campaigns_manager_->GetCampaignsBySlotForTesting(Slot::kNotification)
              ->size());
}

}  // namespace growth

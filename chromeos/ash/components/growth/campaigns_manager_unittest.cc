// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_manager.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/mock_campaigns_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace growth {
namespace {

constexpr char kValidCampaignsFileTemplate[] = R"(
    {
      "reactiveCampaigns": {
        "0": [
          // Invalid targeting.
          {
            "id": 1,
            "targetings": [
              []
            ],
            "payload": {}
          },
          "Invalid campaign",
          {
            "id": 3,
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
      },
      "proactiveCampaigns": {}
    }
)";

constexpr char kValidDemoModeTargeting[] = R"(
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

constexpr char kCampaignsFileName[] = "campaigns.json";

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
  void LoadComponentAndVerifyLoadComplete(
      const base::StringPiece& file_content) {
    TestCampaignsManagerObserver observer;
    campaigns_manager_->AddObserver(&observer);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath campaigns_file(
        temp_dir_.GetPath().Append(kCampaignsFileName));

    base::WriteFile(campaigns_file, file_content);

    EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
        .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
            temp_dir_.GetPath()));

    campaigns_manager_->LoadCampaigns();
    observer.Wait();

    ASSERT_TRUE(observer.load_completed());
  }

  void MockDemoMode(bool in_demo_mode,
                    bool cloud_gaming_device,
                    bool feature_aware_device,
                    const base::StringPiece& store_id,
                    const base::StringPiece& retailer_id,
                    const base::StringPiece& country) {
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
                    const base::StringPiece& store_id,
                    const base::StringPiece& retailer_id,
                    const base::StringPiece& country,
                    const base::Version& app_version) {
    MockDemoMode(in_demo_mode, cloud_gaming_device, feature_aware_device,
                 store_id, retailer_id, country);
    EXPECT_CALL(mock_client_, GetDemoModeAppVersion)
        .WillRepeatedly(testing::ReturnRef(app_version));
  }

  void VerifyDemoModePayload(const Campaign* campaign) {
    const auto* payload = campaign->FindDictByDottedPath("payload.demoModeApp");
    ASSERT_EQ("/asset/peripherals_lang1.mp4",
              *payload->FindStringByDottedPath("attractionLoop.videoSrcLang1"));
    ASSERT_EQ("/asset/peripherals_lang2.mp4",
              *payload->FindStringByDottedPath("attractionLoop.videoSrcLang2"));
  }

  void LoadComponentWithDeviceTargeting(const std::string& milestone_range) {
    auto device_targeting = base::StringPrintf(R"(
            "device": {
              "locales": ["en-US"],
              "milestone": {
                %s
              }
            }
          )",
                                               milestone_range.c_str());
    LoadComponentAndVerifyLoadComplete(base::StringPrintf(
        kValidCampaignsFileTemplate, device_targeting.c_str()));
  }

  base::test::TaskEnvironment task_environment_;
  MockCampaignsManagerClient mock_client_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
  std::unique_ptr<CampaignsManager> campaigns_manager_;

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
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignNoTargeting) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, ""));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  // Verify that the campaign is selected if there is no demo mode targeting.
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignNoTargetingNotInDemoMode) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, ""));

  MockDemoMode(
      /*in_demo_mode=*/false,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  // Verify that the campaign is selected if there is not in demo mode.
  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

// TODO(b/302360652): After user prefs targeting is implemented, add test to
// verify that campaign with user prefs related targeting is not selected when
// user prefs are not available.

TEST_F(CampaignsManagerTest, GetDemoModeCampaignNotInDemoMode) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/false,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignNotGamingDevice) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/false,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignNotFeatureAwareDevice) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/false,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US");

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
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"abc",
      /*country=*/"US");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignCountryMismatch) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, kValidDemoModeTargeting));

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"UK");

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionTargeting) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.0",
        "max": "1.0.0.1"
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
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.0"
      }
    }
  )"));

  const base::Version expected_app_version("1.0.0.3");

  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMinOnlyMismstch) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "min": "1.0.0.3",
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

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMaxOnly) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "max": "1.0.0.3"
      }
    }
  )"));

  const base::Version expected_app_version("1.0.0.3");
  MockDemoMode(
      /*in_demo_mode=*/true,
      /*cloud_gaming_device=*/true,
      /*feature_aware_device=*/true,
      /*store_id=*/"2",
      /*retailer_id=*/"bby",
      /*country=*/"US",
      /*app_version=*/expected_app_version);

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionMaxOnlyMismstch) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "max": "1.0.0.3"
      }
    }
  )"));

  const base::Version expected_app_version("1.0.0.4");
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

TEST_F(CampaignsManagerTest, GetDemoModeCampaignAppVersionInvalidAppVersion) {
  LoadComponentAndVerifyLoadComplete(
      base::StringPrintf(kValidCampaignsFileTemplate, R"(
    "demoMode": {
      "appVersion": {
        "max": "1.0.0.3"
      }
    }
  )"));

  const base::Version expected_app_version = base::Version();
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

TEST_F(CampaignsManagerTest, LoadCampaignsFailed) {
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          absl::nullopt));

  campaigns_manager_->LoadCampaigns();
  observer.Wait();

  ASSERT_TRUE(observer.load_completed());

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, LoadCampaignsInvalidFile) {
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath campaigns_file(temp_dir_.GetPath().Append(kCampaignsFileName));

  base::WriteFile(campaigns_file, "abc");

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          temp_dir_.GetPath()));

  campaigns_manager_->LoadCampaigns();
  observer.Wait();

  ASSERT_TRUE(observer.load_completed());

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, LoadCampaignsEmptyFile) {
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath campaigns_file(temp_dir_.GetPath().Append(kCampaignsFileName));

  base::WriteFile(campaigns_file, "");

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          campaigns_file));

  campaigns_manager_->LoadCampaigns();
  observer.Wait();

  ASSERT_TRUE(observer.load_completed());

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignDeviceTargeting) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(base::StringPrintf(
      R"(
      "min": %d,
      "max": %d
    )",
      current_version, current_version + 1));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMilestoneMinMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(base::StringPrintf(
      R"(
      "min": %d,
      "max": %d
    )",
      current_version + 1, current_version + 1));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMilestoneMaxMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(base::StringPrintf(
      R"(
        "min": %d,
        "max": %d
      )",
      current_version - 2, current_version - 1));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMinMilestoneOnly) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(
      base::StringPrintf(R"("min": %d)", current_version));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMinMilestoneOnlyMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(
      base::StringPrintf(R"("min": %d)", current_version + 1));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMaxMilestoneOnly) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(
      base::StringPrintf(R"("max": %d)", current_version));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  VerifyDemoModePayload(
      campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignMaxMilestoneOnlyMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(
      base::StringPrintf(R"("max": %d)", current_version - 1));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-US")));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

TEST_F(CampaignsManagerTest, GetCampaignApplicationLocaleMismatch) {
  auto current_version = version_info::GetMajorVersionNumberAsInt();
  LoadComponentWithDeviceTargeting(
      base::StringPrintf(R"("max": %d)", current_version));
  EXPECT_CALL(mock_client_, GetApplicationLocale())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::string("en-CA")));

  ASSERT_EQ(nullptr, campaigns_manager_->GetCampaignBySlot(Slot::kDemoModeApp));
}

}  // namespace growth

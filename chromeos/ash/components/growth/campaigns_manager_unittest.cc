// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_manager.h"
#include <memory>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/growth/mock_campaigns_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;

namespace growth {
namespace {

constexpr char kValidCampaignsFile[] = R"(
    {
      "reactiveCampaigns": {
        "0": [
          {
            "id": 1,
            "targetings": [
              {
                "deviceTargeting": {
                  "langs": ["en-AU"]
                },
                "demoModeTargeting": {
                  "storeIds": [
                    "2",
                    "4",
                    "6"
                  ],
                  "country": "US"
                }
              }
            ],
            "payload": {
              "demoMode": {
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
    campaigns_manager_ = std::make_unique<CampaignsManager>(&mock_client_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockCampaignsManagerClient mock_client_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<CampaignsManager> campaigns_manager_;
};

TEST_F(CampaignsManagerTest, LoadCampiagns) {
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath campaigns_file(temp_dir_.GetPath().Append(kCampaignsFileName));

  base::WriteFile(campaigns_file, kValidCampaignsFile);

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          campaigns_file));

  campaigns_manager_->LoadCampaigns();
  observer.Wait();

  ASSERT_TRUE(observer.load_completed());
}

TEST_F(CampaignsManagerTest, LoadCampiagnsFailed) {
  TestCampaignsManagerObserver observer;
  campaigns_manager_->AddObserver(&observer);

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  EXPECT_CALL(mock_client_, LoadCampaignsComponent(_))
      .WillOnce(InvokeCallbackArgument<0, CampaignComponentLoadedCallback>(
          absl::nullopt));

  campaigns_manager_->LoadCampaigns();
  observer.Wait();

  ASSERT_TRUE(observer.load_completed());
}

// TODO(b/298467438): Verify getting campaign for a given slot.

}  // namespace growth

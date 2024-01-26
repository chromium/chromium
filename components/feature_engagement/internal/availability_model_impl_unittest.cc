// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/availability_model_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/feature_engagement/internal/persistent_availability_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kTestFeatureFoo, "test_foo", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureBar, "test_bar", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureQux, "test_qux", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureNop, "test_nop", base::FEATURE_DISABLED_BY_DEFAULT);

class AvailabilityModelImplTest : public testing::Test {
 public:
  AvailabilityModelImplTest() {
    initialized_callback_ = base::BindOnce(
        &AvailabilityModelImplTest::OnInitialized, base::Unretained(this));
  }

  AvailabilityModelImplTest(const AvailabilityModelImplTest&) = delete;
  AvailabilityModelImplTest& operator=(const AvailabilityModelImplTest&) =
      delete;

  ~AvailabilityModelImplTest() override = default;

  // SetUpModel exists so that the filter can be changed for any test.
  void SetUpModel(
      bool success,
      std::unique_ptr<std::map<std::string, uint32_t>> store_content) {
    auto store_loader = base::BindOnce(&AvailabilityModelImplTest::StoreLoader,
                                       base::Unretained(this), success,
                                       std::move(store_content));
    availability_model_ =
        std::make_unique<AvailabilityModelImpl>(std::move(store_loader));
  }

  void OnInitialized(bool success) { success_ = success; }

  void StoreLoader(
      bool success,
      std::unique_ptr<std::map<std::string, uint32_t>> store_content,
      PersistentAvailabilityStore::OnLoadedCallback callback,
      uint32_t current_day) {
    current_day_ = current_day;
    std::move(callback).Run(success, std::move(store_content));
  }

 protected:
  std::unique_ptr<AvailabilityModelImpl> availability_model_;

  AvailabilityModel::OnInitializedCallback initialized_callback_;
  std::optional<bool> success_;
  std::optional<uint32_t> current_day_;
};

}  // namespace

TEST_F(AvailabilityModelImplTest, InitializationSuccess) {
  SetUpModel(true, std::make_unique<std::map<std::string, uint32_t>>());
  EXPECT_FALSE(availability_model_->IsReady());
  availability_model_->Initialize(std::move(initialized_callback_), 14u);
  EXPECT_TRUE(availability_model_->IsReady());
  EXPECT_TRUE(success_.has_value());
  EXPECT_TRUE(success_.value());
  EXPECT_EQ(14u, current_day_);
}

TEST_F(AvailabilityModelImplTest, InitializationFailed) {
  SetUpModel(false, std::make_unique<std::map<std::string, uint32_t>>());
  EXPECT_FALSE(availability_model_->IsReady());
  availability_model_->Initialize(std::move(initialized_callback_), 14u);
  EXPECT_FALSE(availability_model_->IsReady());
  EXPECT_TRUE(success_.has_value());
  EXPECT_FALSE(success_.value());
  EXPECT_EQ(14u, current_day_);
}

TEST_F(AvailabilityModelImplTest, SuccessfullyLoadThreeFeatures) {
  auto availabilities = std::make_unique<std::map<std::string, uint32_t>>();
  availabilities->insert(std::make_pair(kTestFeatureFoo.name, 100u));
  availabilities->insert(std::make_pair(kTestFeatureBar.name, 200u));
  availabilities->insert(std::make_pair(kTestFeatureNop.name, 300u));

  SetUpModel(true, std::move(availabilities));
  availability_model_->Initialize(std::move(initialized_callback_), 14u);
  EXPECT_TRUE(availability_model_->IsReady());

  EXPECT_EQ(100u, availability_model_->GetAvailability(kTestFeatureFoo));
  EXPECT_EQ(200u, availability_model_->GetAvailability(kTestFeatureBar));
  EXPECT_EQ(300u, availability_model_->GetAvailability(kTestFeatureNop));
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kTestFeatureQux));
}

TEST_F(AvailabilityModelImplTest, FailToLoadThreeFeatures) {
  auto availabilities = std::make_unique<std::map<std::string, uint32_t>>();
  availabilities->insert(std::make_pair(kTestFeatureFoo.name, 100u));
  availabilities->insert(std::make_pair(kTestFeatureBar.name, 200u));
  availabilities->insert(std::make_pair(kTestFeatureNop.name, 300u));

  SetUpModel(false, std::move(availabilities));
  availability_model_->Initialize(std::move(initialized_callback_), 14u);
  EXPECT_FALSE(availability_model_->IsReady());

  // Load failed, so all results should be ignored.
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kTestFeatureFoo));
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kTestFeatureBar));
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kTestFeatureNop));
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kTestFeatureQux));
}

}  // namespace feature_engagement

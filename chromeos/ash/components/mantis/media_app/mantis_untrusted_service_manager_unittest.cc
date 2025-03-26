// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service_manager.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/mantis/mojom/mantis_processor.mojom.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {
namespace {

using ::ash::media_app_ui::mojom::MantisUntrustedServiceResultPtr;
using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::mantis::mojom::InitializeResult;
using ::mantis::mojom::MantisFeatureStatus;
using ::mantis::mojom::PlatformModelProgressObserver;
using ::specialized_features::FeatureAccessChecker;
using ::specialized_features::FeatureAccessConfig;
using ::specialized_features::FeatureAccessFailure;
using ::specialized_features::FeatureAccessFailureSet;
using ::testing::NiceMock;
using ::testing::Return;

enum class GenAIPhotoEditingSettings {
  kAllowed = 0,                // Allow and improve AI models
  kAllowedWithoutLogging = 1,  // Allow without improving AI models
  kDisabled = 2,               // Do not allow
};

class MockFeatureAccessChecker
    : public specialized_features::FeatureAccessChecker {
 public:
  MOCK_METHOD(FeatureAccessFailureSet, Check, (), (const override));
};

class MockMojoMantisService
    : public mantis::mojom::MantisService,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  MockMojoMantisService() : mantis_receiver_(this), provider_receiver_(this) {
    ash::mojo_service_manager::GetServiceManagerProxy()->Register(
        chromeos::mojo_services::kCrosMantisService,
        provider_receiver_.BindNewPipeAndPassRemote());
  }

  // Implements mantis::mojom::MantisService:
  MOCK_METHOD(void,
              GetMantisFeatureStatus,
              (GetMantisFeatureStatusCallback),
              (override));
  MOCK_METHOD(
      void,
      Initialize,
      (mojo::PendingRemote<mantis::mojom::PlatformModelProgressObserver>,
       mojo::PendingReceiver<mantis::mojom::MantisProcessor>,
       const std::optional<base::Uuid>&,
       mojo::PendingRemote<chromeos::machine_learning::mojom::TextClassifier>,
       InitializeCallback),
      (override));

  // Implements chromeos::mojo_service_manager::mojom::ServiceProvider:
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr client_identity,
      mojo::ScopedMessagePipeHandle handle) override {
    CHECK(handle->is_valid());
    CHECK(!mantis_receiver_.is_bound());
    mantis_receiver_.Bind(
        mojo::PendingReceiver<mantis::mojom::MantisService>(std::move(handle)));
  }

 private:
  mojo_service_manager::FakeMojoServiceManager fake_mojo_service_manager_;
  mojo::Receiver<mantis::mojom::MantisService> mantis_receiver_;
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_;
};

class MockMantisUntrustedPage
    : public media_app_ui::mojom::MantisUntrustedPage {
 public:
  MockMantisUntrustedPage() : page_(this) {}

  MOCK_METHOD(void, ReportMantisProgress, (double), (override));

  mojo::PendingRemote<media_app_ui::mojom::MantisUntrustedPage>
  BindNewPipeAndPassRemote() {
    return page_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<media_app_ui::mojom::MantisUntrustedPage> page_;
};

class MantisUntrustedServiceManagerTest : public testing::Test {
 public:
  MantisUntrustedServiceManagerTest()
      : mock_mojo_service_(std::make_unique<NiceMock<MockMojoMantisService>>()),
        access_checker_(
            std::make_unique<NiceMock<MockFeatureAccessChecker>>()) {
    // TODO(crbug.com/388786784): Move enterprise policy handling to
    // FeatureAccessChecker.
    pref_.registry()->RegisterIntegerPref(
        ash::prefs::kGenAIPhotoEditingSettings,
        static_cast<int>(GenAIPhotoEditingSettings::kAllowed));

    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();

    // Set Mantis is available by default in this test, so that we can check
    // each unavailable case one by one.
    ON_CALL(*access_checker_, Check)
        .WillByDefault(Return(FeatureAccessFailureSet()));
    ON_CALL(*mock_mojo_service_, GetMantisFeatureStatus)
        .WillByDefault(RunOnceCallback<0>(MantisFeatureStatus::kAvailable));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NiceMock<MockMojoMantisService>> mock_mojo_service_;
  std::unique_ptr<NiceMock<MockFeatureAccessChecker>> access_checker_;
  TestingPrefServiceSimple pref_;
  chromeos::machine_learning::FakeServiceConnectionImpl fake_connection_;
};

TEST_F(MantisUntrustedServiceManagerTest, IsAvailable) {
  // Mantis is set to be available by default in the test fixture.
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<bool> result_future;
  manager.IsAvailable(&pref_, result_future.GetCallback());

  EXPECT_TRUE(result_future.Take());
}

TEST_F(MantisUntrustedServiceManagerTest, IsNotAvailableByFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kMediaAppImageMantisModel});

  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<bool> result_future;
  manager.IsAvailable(&pref_, result_future.GetCallback());

  EXPECT_FALSE(result_future.Take());
}

TEST_F(MantisUntrustedServiceManagerTest, IsNotAvailableByAccountCapabilities) {
  ASSERT_FALSE(MantisUntrustedServiceManager::GetFeatureAccessConfig()
                   .capability_callback.is_null());
  EXPECT_CALL(*access_checker_, Check)
      .WillOnce(Return(FeatureAccessFailureSet(
          {FeatureAccessFailure::kAccountCapabilitiesCheckFailed})));
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<bool> result_future;
  manager.IsAvailable(&pref_, result_future.GetCallback());

  EXPECT_FALSE(result_future.Take());
}

TEST_F(MantisUntrustedServiceManagerTest, IsNotAvailableByEnterprisePolicy) {
  pref_.SetInteger(ash::prefs::kGenAIPhotoEditingSettings,
                   static_cast<int>(GenAIPhotoEditingSettings::kDisabled));
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<bool> result_future;
  manager.IsAvailable(&pref_, result_future.GetCallback());

  EXPECT_FALSE(result_future.Take());
}

TEST_F(MantisUntrustedServiceManagerTest, IsNotAvailableByUnavailableService) {
  // Reset mantis mojo service but keep mojo service manager.
  mock_mojo_service_.reset();
  mojo_service_manager::FakeMojoServiceManager fake_mojo_service_manager;
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<bool> result_future;
  manager.IsAvailable(&pref_, result_future.GetCallback());

  EXPECT_FALSE(result_future.Take());
}

TEST_F(MantisUntrustedServiceManagerTest, IsNotAvailableByMantisFeatureStatus) {
  ON_CALL(*mock_mojo_service_, GetMantisFeatureStatus)
      .WillByDefault(
          RunOnceCallback<0>(MantisFeatureStatus::kDeviceNotSupported));
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<bool> result_future;
  manager.IsAvailable(&pref_, result_future.GetCallback());

  EXPECT_FALSE(result_future.Take());
}

TEST_F(MantisUntrustedServiceManagerTest, CreateSuccess) {
  constexpr double kProgress = 1.0;
  EXPECT_CALL(*mock_mojo_service_, Initialize)
      .WillOnce(testing::WithArgs<0, 4>(
          [](mojo::PendingRemote<PlatformModelProgressObserver>
                 pending_observer,
             base::OnceCallback<void(InitializeResult)> callback) {
            mojo::Remote<PlatformModelProgressObserver> observer(
                std::move(pending_observer));
            // To check if progress report is passed.
            observer->Progress(kProgress);
            observer.FlushForTesting();
            std::move(callback).Run(InitializeResult::kSuccess);
          }));
  MockMantisUntrustedPage page;
  EXPECT_CALL(page, ReportMantisProgress(kProgress));
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  TestFuture<MantisUntrustedServiceResultPtr> result_future;
  manager.Create(page.BindNewPipeAndPassRemote(), std::nullopt,
                 result_future.GetCallback());

  MantisUntrustedServiceResultPtr result = result_future.Take();
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->is_service());
}

TEST_F(MantisUntrustedServiceManagerTest, CreateFailed) {
  EXPECT_CALL(*mock_mojo_service_, Initialize)
      .WillOnce(RunOnceCallback<4>(InitializeResult::kFailedToLoadLibrary));
  MantisUntrustedServiceManager manager(std::move(access_checker_));

  MockMantisUntrustedPage page;
  TestFuture<MantisUntrustedServiceResultPtr> result_future;
  manager.Create(page.BindNewPipeAndPassRemote(), std::nullopt,
                 result_future.GetCallback());

  MantisUntrustedServiceResultPtr result = result_future.Take();
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->is_error());
}

}  // namespace
}  // namespace ash

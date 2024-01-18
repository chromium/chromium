// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/sensor_info/sensor_page_handler.h"

#include <memory>
#include <string>

#include "ash/sensor_info/sensor_provider.h"
#include "ash/sensor_info/sensor_types.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// This Timestamp can be converted to 2023-07-02 05:00:00.000 UTC
constexpr int64_t kTestTimestamp = 1688274000;

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  return profile;
}

}  // namespace

class SensorPageHandlerTest : public testing::Test {
 public:
  SensorPageHandlerTest()
      : profile_(
            MakeTestingProfile(test_url_loader_factory_.GetSafeWeakWrapper())) {
  }
  ~SensorPageHandlerTest() override = default;

 protected:
  void SetUp() override {
    file_path_ = base::GetTempDirForTesting().Append("sensor_info.txt");
    provider_ = std::make_unique<ash::SensorProvider>();
    page_handler_ = std::make_unique<ash::SensorPageHandler>(
        profile_.get(), provider_.get(),
        mojo::PendingReceiver<sensor::mojom::PageHandler>(), file_path_);
  }
  network::TestURLLoaderFactory test_url_loader_factory_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ash::SensorProvider> provider_;
  std::unique_ptr<ash::SensorPageHandler> page_handler_;
  base::FilePath file_path_;
};

TEST_F(SensorPageHandlerTest, GenerateString) {
  ash::SensorUpdate test_update;
  test_update.Set(ash::SensorType::kLidAngle, 90);
  test_update.Set(ash::SensorType::kAccelerometerBase, -1, 1, 2);
  base::Time time = base::Time::FromTimeT(kTestTimestamp);
  std::string output = page_handler_->GenerateString(test_update, time);
  constexpr char EXPECT[] =
      "2023-07-02 05:00:00.000 UTC\n90.00\n-1.00 1.00 "
      "2.00\nNone.\nNone.\nNone.\n";
  ASSERT_EQ(output, EXPECT);
}

TEST_F(SensorPageHandlerTest, StartRecordingUpdate) {
  page_handler_->StartRecordingUpdate();
  task_environment_.RunUntilIdle();
  base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(file.IsValid());
}

TEST_F(SensorPageHandlerTest, StopRecordingUpdate) {
  page_handler_->StartRecordingUpdate();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(page_handler_->CheckOutFileForTesting());
  page_handler_->StopRecordingUpdate();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(page_handler_->CheckOutFileForTesting());
}

TEST_F(SensorPageHandlerTest, StartThenStop) {
  page_handler_->StartRecordingUpdate();
  page_handler_->StopRecordingUpdate();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(page_handler_->CheckOutFileForTesting());
  EXPECT_TRUE(page_handler_->state_ == SensorPageHandler::State::kStopped);
  EXPECT_FALSE(page_handler_->save_update_);
}

TEST_F(SensorPageHandlerTest, StartThenStopThenStart) {
  page_handler_->StartRecordingUpdate();
  page_handler_->StopRecordingUpdate();
  page_handler_->StartRecordingUpdate();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(page_handler_->CheckOutFileForTesting());
  EXPECT_TRUE(page_handler_->state_ == SensorPageHandler::State::kOpened);
  EXPECT_TRUE(page_handler_->save_update_);
}
}  // namespace ash

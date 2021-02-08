// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/app_restore_data.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace full_restore {

namespace {

constexpr char kAppId[] = "aaa";

constexpr int32_t kId1 = 100;
constexpr int32_t kId2 = 200;

constexpr int32_t kActivationIndex1 = 100;
constexpr int32_t kActivationIndex2 = 101;

}  // namespace

// Unit tests for restore data.
class FullRestoreReadAndSaveTest : public testing::Test {
 public:
  FullRestoreReadAndSaveTest() = default;
  ~FullRestoreReadAndSaveTest() override = default;

  FullRestoreReadAndSaveTest(const FullRestoreReadAndSaveTest&) = delete;
  FullRestoreReadAndSaveTest& operator=(const FullRestoreReadAndSaveTest&) =
      delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& GetPath() { return tmp_dir_.GetPath(); }

  void ReadFromFile(const base::FilePath& file_path) {
    FullRestoreReadHandler* read_handler =
        FullRestoreReadHandler::GetInstance();
    base::RunLoop run_loop;

    read_handler->ReadFromFile(
        file_path, base::BindLambdaForTesting(
                       [&](std::unique_ptr<RestoreData> restore_data) {
                         run_loop.Quit();
                         restore_data_ = std::move(restore_data);
                       }));
    run_loop.Run();
  }

  const RestoreData* GetRestoreData(const base::FilePath& file_path) {
    return restore_data_.get();
  }

  void AddAppLaunchInfo(const base::FilePath& file_path, int32_t id) {
    full_restore::SaveAppLaunchInfo(
        file_path, std::make_unique<full_restore::AppLaunchInfo>(kAppId, id));
  }

  void CreateWindowInfo(int32_t id, int32_t index) {
    std::unique_ptr<aura::Window> window(
        aura::test::CreateTestWindowWithId(id, nullptr));
    WindowInfo window_info;
    window_info.window = window.get();
    window->SetProperty(full_restore::kWindowIdKey, id);
    window_info.activation_index = index;
    full_restore::SaveWindowInfo(window_info);
  }

  void VerifyRestoreData(const base::FilePath& file_path,
                         int32_t id,
                         int32_t index) {
    ReadFromFile(file_path);

    const auto* restore_data = GetRestoreData(file_path);
    ASSERT_TRUE(restore_data != nullptr);

    const auto& launch_list = restore_data->app_id_to_launch_list();
    EXPECT_EQ(1u, launch_list.size());

    // Verify for |kAppId|
    const auto launch_list_it = launch_list.find(kAppId);
    EXPECT_TRUE(launch_list_it != launch_list.end());
    EXPECT_EQ(1u, launch_list_it->second.size());

    // Verify for |id|
    const auto app_restore_data_it = launch_list_it->second.find(id);
    EXPECT_TRUE(app_restore_data_it != launch_list_it->second.end());

    const auto& data = app_restore_data_it->second;
    EXPECT_TRUE(data->activation_index.has_value());
    EXPECT_EQ(index, data->activation_index.value());
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir tmp_dir_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<RestoreData> restore_data_;
};

TEST_F(FullRestoreReadAndSaveTest, ReadEmptyRestoreData) {
  ReadFromFile(GetPath());
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  ASSERT_TRUE(restore_data->app_id_to_launch_list().empty());
}

TEST_F(FullRestoreReadAndSaveTest, SaveAndReadRestoreData) {
  FullRestoreSaveHandler* save_handler = FullRestoreSaveHandler::GetInstance();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add app launch info, and verify the timer starts.
  AddAppLaunchInfo(GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Add one more app launch info, and verify the timer is still running.
  AddAppLaunchInfo(GetPath(), kId2);
  EXPECT_TRUE(timer->IsRunning());

  // Simulate timeout, and verify the timer stops.
  timer->FireNow();
  CreateWindowInfo(kId2, kActivationIndex2);
  task_environment().RunUntilIdle();
  EXPECT_FALSE(timer->IsRunning());

  // Modify the window info, and verify the timer starts.
  CreateWindowInfo(kId1, kActivationIndex1);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify the restore data can be read correctly.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);

  const auto& launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list.size());

  // Verify for |kAppId|
  const auto launch_list_it = launch_list.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(2u, launch_list_it->second.size());

  // Verify for |kId1|
  const auto app_restore_data_it1 = launch_list_it->second.find(kId1);
  EXPECT_TRUE(app_restore_data_it1 != launch_list_it->second.end());

  const auto& data1 = app_restore_data_it1->second;
  EXPECT_TRUE(data1->activation_index.has_value());
  EXPECT_EQ(kActivationIndex1, data1->activation_index.value());

  // Verify for |kId2|
  const auto app_restore_data_it2 = launch_list_it->second.find(kId2);
  EXPECT_TRUE(app_restore_data_it2 != launch_list_it->second.end());

  const auto& data2 = app_restore_data_it2->second;
  EXPECT_TRUE(data2->activation_index.has_value());
  EXPECT_EQ(kActivationIndex2, data2->activation_index.value());
}

TEST_F(FullRestoreReadAndSaveTest, MultipleFilePaths) {
  FullRestoreSaveHandler* save_handler = FullRestoreSaveHandler::GetInstance();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  base::ScopedTempDir tmp_dir1;
  base::ScopedTempDir tmp_dir2;
  ASSERT_TRUE(tmp_dir1.CreateUniqueTempDir());
  ASSERT_TRUE(tmp_dir2.CreateUniqueTempDir());

  // Add app launch info for |tmp_dir1|, and verify the timer starts.
  AddAppLaunchInfo(tmp_dir1.GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Add app launch info for |tmp_dir2|, and verify the timer is still running.
  AddAppLaunchInfo(tmp_dir2.GetPath(), kId2);
  EXPECT_TRUE(timer->IsRunning());

  // Simulate timeout, and verify the timer stops.
  timer->FireNow();
  CreateWindowInfo(kId2, kActivationIndex2);
  task_environment().RunUntilIdle();
  EXPECT_FALSE(timer->IsRunning());

  // Modify the window info, and verify the timer starts.
  CreateWindowInfo(kId1, kActivationIndex1);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  VerifyRestoreData(tmp_dir1.GetPath(), kId1, kActivationIndex1);
  VerifyRestoreData(tmp_dir2.GetPath(), kId2, kActivationIndex2);
}

}  // namespace full_restore

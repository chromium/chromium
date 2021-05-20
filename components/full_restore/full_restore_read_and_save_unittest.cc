// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/public/cpp/app_types.h"
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
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace full_restore {

namespace {

constexpr char kAppId[] = "aaa";

constexpr int32_t kId1 = 100;
constexpr int32_t kId2 = 200;

constexpr int32_t kActivationIndex1 = 100;
constexpr int32_t kActivationIndex2 = 101;

constexpr int32_t kArcSessionId1 = 1;
constexpr int32_t kArcSessionId2 = kArcSessionIdOffsetForRestoredLaunching + 1;

constexpr int32_t kArcTaskId1 = 666;
constexpr int32_t kArcTaskId2 = 888;

}  // namespace

class FullRestoreReadHandlerTestApi {
 public:
  explicit FullRestoreReadHandlerTestApi(FullRestoreReadHandler* read_handler)
      : read_handler_(read_handler) {}

  FullRestoreReadHandlerTestApi(const FullRestoreReadHandlerTestApi&) = delete;
  FullRestoreReadHandlerTestApi& operator=(
      const FullRestoreReadHandlerTestApi&) = delete;
  ~FullRestoreReadHandlerTestApi() = default;

  const ArcReadHandler* GetArcReadHander() const {
    DCHECK(read_handler_);
    return read_handler_->arc_read_handler_.get();
  }

  const std::map<int32_t, std::string>& GetArcWindowIdMap() const {
    const auto* arc_read_handler = GetArcReadHander();
    DCHECK(arc_read_handler);
    return arc_read_handler->window_id_to_app_id_;
  }

  const std::map<int32_t, int32_t>& GetArcSessionIdMap() const {
    const auto* arc_read_handler = GetArcReadHander();
    DCHECK(arc_read_handler);
    return arc_read_handler->session_id_to_window_id_;
  }

  const std::map<int32_t, int32_t>& GetArcTaskIdMap() const {
    const auto* arc_read_handler = GetArcReadHander();
    DCHECK(arc_read_handler);
    return arc_read_handler->task_id_to_window_id_;
  }

 private:
  FullRestoreReadHandler* read_handler_;
};

class FullRestoreSaveHandlerTestApi {
 public:
  explicit FullRestoreSaveHandlerTestApi(FullRestoreSaveHandler* save_handler)
      : save_handler_(save_handler) {}

  FullRestoreSaveHandlerTestApi(const FullRestoreSaveHandlerTestApi&) = delete;
  FullRestoreSaveHandlerTestApi& operator=(
      const FullRestoreSaveHandlerTestApi&) = delete;
  ~FullRestoreSaveHandlerTestApi() = default;

  const ArcSaveHandler* GetArcSaveHander() const {
    DCHECK(save_handler_);
    return save_handler_->arc_save_handler_.get();
  }

  const ArcSaveHandler::SessionIdMap& GetArcSessionIdMap() const {
    const auto* arc_save_handler = GetArcSaveHander();
    DCHECK(arc_save_handler);
    return arc_save_handler->session_id_to_app_launch_info_;
  }

  const std::map<int32_t, std::string>& GetArcTaskIdMap() const {
    const auto* arc_save_handler = GetArcSaveHander();
    DCHECK(arc_save_handler);
    return arc_save_handler->task_id_to_app_id_;
  }

  void ModifyLaunchTime(int32_t session_id) {
    auto& session_id_to_app_launch_info =
        arc_save_handler()->session_id_to_app_launch_info_;
    auto it = session_id_to_app_launch_info.find(session_id);
    if (it == session_id_to_app_launch_info.end())
      return;

    // If there is no task created for the session id in 600 seconds, the
    // session id record is removed. So set the record time as 601 seconds ago,
    // so that CheckTasksForAppLaunching can remove the session id record to
    // simulate the task is not created for the session id.
    it->second.second = it->second.second - base::TimeDelta::FromSeconds(601);
  }

  base::RepeatingTimer* GetArcCheckTimer() {
    return &arc_save_handler()->check_timer_;
  }

  void CheckArcTasks() { arc_save_handler()->CheckTasksForAppLaunching(); }

 private:
  ArcSaveHandler* arc_save_handler() {
    DCHECK(save_handler_);
    DCHECK(save_handler_->arc_save_handler_.get());
    return save_handler_->arc_save_handler_.get();
  }

  FullRestoreSaveHandler* save_handler_;
};

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

  void AddArcAppLaunchInfo(const base::FilePath& file_path) {
    full_restore::SaveAppLaunchInfo(
        file_path,
        std::make_unique<full_restore::AppLaunchInfo>(
            kAppId, /*event_flags=*/0, kArcSessionId1, /*display_id*/ 0));
  }

  void CreateWindowInfo(int32_t id,
                        int32_t index,
                        ash::AppType app_type = ash::AppType::BROWSER) {
    std::unique_ptr<aura::Window> window(
        aura::test::CreateTestWindowWithId(id, nullptr));
    WindowInfo window_info;
    window_info.window = window.get();
    window->SetProperty(aura::client::kAppType, static_cast<int>(app_type));
    window->SetProperty(full_restore::kWindowIdKey, id);
    window_info.activation_index = index;
    full_restore::SaveWindowInfo(window_info);
  }

  std::unique_ptr<WindowInfo> GetArcWindowInfo(int32_t restore_window_id) {
    std::unique_ptr<aura::Window> window(
        aura::test::CreateTestWindowWithId(restore_window_id, nullptr));
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::ARC_APP));
    window->SetProperty(full_restore::kRestoreWindowIdKey, restore_window_id);
    return full_restore::GetWindowInfo(window.get());
  }

  void VerifyRestoreData(const base::FilePath& file_path,
                         int32_t id,
                         int32_t index) {
    ReadFromFile(file_path);

    const auto* restore_data = GetRestoreData(file_path);
    ASSERT_TRUE(restore_data != nullptr);

    const auto& launch_list = restore_data->app_id_to_launch_list();
    EXPECT_EQ(1u, launch_list.size());

    // Verify for |kAppId|.
    const auto launch_list_it = launch_list.find(kAppId);
    EXPECT_TRUE(launch_list_it != launch_list.end());
    EXPECT_EQ(1u, launch_list_it->second.size());

    // Verify for |id|.
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

  // Verify for |kAppId|.
  const auto launch_list_it = launch_list.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(2u, launch_list_it->second.size());

  // Verify for |kId1|.
  const auto app_restore_data_it1 = launch_list_it->second.find(kId1);
  EXPECT_TRUE(app_restore_data_it1 != launch_list_it->second.end());

  const auto& data1 = app_restore_data_it1->second;
  EXPECT_TRUE(data1->activation_index.has_value());
  EXPECT_EQ(kActivationIndex1, data1->activation_index.value());

  // Verify for |kId2|.
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

TEST_F(FullRestoreReadAndSaveTest, ArcWindowSaving) {
  FullRestoreSaveHandler* save_handler = FullRestoreSaveHandler::GetInstance();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add an ARC app launch info.
  AddArcAppLaunchInfo(GetPath());
  const ArcSaveHandler* arc_save_handler = test_api.GetArcSaveHander();
  ASSERT_TRUE(arc_save_handler);
  const auto& arc_session_id_map = test_api.GetArcSessionIdMap();
  EXPECT_EQ(1u, arc_session_id_map.size());
  auto session_it = arc_session_id_map.find(kArcSessionId1);
  EXPECT_TRUE(session_it != arc_session_id_map.end());

  // Create a task. Since we have got the task, the arc session id map can be
  // cleared.
  save_handler->OnTaskCreated(kAppId, kArcTaskId1, kArcSessionId1);
  EXPECT_TRUE(arc_session_id_map.empty());
  const auto& task_id_map = test_api.GetArcTaskIdMap();
  EXPECT_EQ(1u, task_id_map.size());
  auto task_id = task_id_map.find(kArcTaskId1);
  EXPECT_TRUE(task_id != task_id_map.end());

  // Destroy the task.
  save_handler->OnTaskDestroyed(kArcTaskId1);
  EXPECT_TRUE(task_id_map.empty());

  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify there is not restore data.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  EXPECT_TRUE(restore_data->app_id_to_launch_list().empty());
}

TEST_F(FullRestoreReadAndSaveTest, ArcLaunchWithoutTask) {
  FullRestoreSaveHandler* save_handler = FullRestoreSaveHandler::GetInstance();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add an ARC app launch info.
  AddArcAppLaunchInfo(GetPath());

  // Verify the ARC app launch info is saved to |arc_session_id_map|.
  const auto& arc_session_id_map = test_api.GetArcSessionIdMap();
  EXPECT_EQ(1u, arc_session_id_map.size());
  auto session_it = arc_session_id_map.find(kArcSessionId1);
  EXPECT_TRUE(session_it != arc_session_id_map.end());

  // Verify the ARC check timer starts running.
  base::RepeatingTimer* arc_check_timer = test_api.GetArcCheckTimer();
  EXPECT_TRUE(arc_check_timer->IsRunning());

  // Simulate more than 30 seconds have passed, OnTaskCreated is not called, and
  // the ARC check timer is expired to remove the ARC app launch info.
  test_api.ModifyLaunchTime(kArcSessionId1);
  test_api.CheckArcTasks();
  EXPECT_TRUE(arc_session_id_map.empty());
  EXPECT_TRUE(test_api.GetArcTaskIdMap().empty());
  EXPECT_FALSE(arc_check_timer->IsRunning());

  // Verify the timer in FullRestoreSaveHandler is not running, because there is
  // no app launching info to save.
  EXPECT_FALSE(timer->IsRunning());
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify there is not restore data.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  EXPECT_TRUE(restore_data->app_id_to_launch_list().empty());
}

TEST_F(FullRestoreReadAndSaveTest, ArcWindowRestore) {
  FullRestoreSaveHandler* save_handler = FullRestoreSaveHandler::GetInstance();
  FullRestoreSaveHandlerTestApi save_test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add an ARC app launch info.
  AddArcAppLaunchInfo(GetPath());
  const ArcSaveHandler* arc_save_handler = save_test_api.GetArcSaveHander();
  ASSERT_TRUE(arc_save_handler);
  EXPECT_EQ(1u, save_test_api.GetArcSessionIdMap().size());

  // Verify the ARC check timer starts running.
  base::RepeatingTimer* arc_check_timer = save_test_api.GetArcCheckTimer();
  EXPECT_TRUE(arc_check_timer->IsRunning());

  // Create a task. Since we have got the task, the arc session id map can be
  // cleared.
  save_handler->OnTaskCreated(kAppId, kArcTaskId1, kArcSessionId1);
  EXPECT_TRUE(save_test_api.GetArcSessionIdMap().empty());
  EXPECT_EQ(1u, save_test_api.GetArcTaskIdMap().size());
  EXPECT_FALSE(arc_check_timer->IsRunning());

  // Modify the window info.
  CreateWindowInfo(kArcTaskId1, kActivationIndex1, ash::AppType::ARC_APP);
  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify the restore data can be read correctly.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);

  FullRestoreReadHandler* read_handler = FullRestoreReadHandler::GetInstance();
  FullRestoreReadHandlerTestApi read_test_api(read_handler);
  ASSERT_TRUE(read_test_api.GetArcReadHander());
  EXPECT_EQ(1u, read_test_api.GetArcWindowIdMap().size());

  // Verify the map from app ids to launch list:
  // std::map<app_id, std::map<window_id, std::unique_ptr<AppRestoreData>>>
  const auto& launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list.size());

  // Verify the launch list for |kAppId|:
  // std::map<window_id, std::unique_ptr<AppRestoreData>>
  const auto launch_list_it = launch_list.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(1u, launch_list_it->second.size());

  // Verify that there is an AppRestoreData for the window id |kArcTaskId1|.
  const auto app_restore_data_it = launch_list_it->second.find(kArcTaskId1);
  EXPECT_TRUE(app_restore_data_it != launch_list_it->second.end());

  // Verify the AppRestoreData.
  const std::unique_ptr<AppRestoreData>& data = app_restore_data_it->second;
  EXPECT_TRUE(data->activation_index.has_value());
  EXPECT_EQ(kActivationIndex1, data->activation_index.value());

  // Simulate the ARC app launching, and set the arc session id kArcSessionId2
  // for the restore window id |kArcTaskId1|.
  read_handler->SetArcSessionIdForWindowId(kArcSessionId2, kArcTaskId1);
  EXPECT_EQ(1u, read_test_api.GetArcSessionIdMap().size());

  // Before OnTaskCreated is called, return -1 to add the ARC app window to the
  // hidden container.
  EXPECT_EQ(kParentToHiddenContainer,
            full_restore::GetArcRestoreWindowId(kArcTaskId2));

  // Call OnTaskCreated to simulate that the ARC app with |kAppId| has been
  // launched, and the new task id |kArcTaskId2| has been created with
  // |kArcSessionId2| returned.
  read_handler->OnTaskCreated(kAppId, kArcTaskId2, kArcSessionId2);
  EXPECT_EQ(1u, read_test_api.GetArcTaskIdMap().size());

  // Since we have got the new task with |kArcSessionId2|, the arc session id
  // map can be cleared. And verify that we can get the restore window id
  // |kArcTaskId1| with the new |kArcTaskId2|.
  EXPECT_TRUE(read_test_api.GetArcSessionIdMap().empty());
  EXPECT_EQ(kArcTaskId1, full_restore::GetArcRestoreWindowId(kArcTaskId2));

  // Verify |window_info| for |kArcTaskId1|.
  auto window_info = GetArcWindowInfo(kArcTaskId1);
  EXPECT_TRUE(window_info);
  EXPECT_EQ(kActivationIndex1, window_info->activation_index);

  // Call OnTaskDestroyed to simulate the ARC app launching has been finished
  // for |kArcTaskId2|, and verify the task id map is now empty and a invalid
  // value is returned when trying to get the restore window id.
  read_handler->OnTaskDestroyed(kArcTaskId2);
  EXPECT_EQ(0, full_restore::GetArcRestoreWindowId(kArcTaskId2));
  EXPECT_TRUE(read_test_api.GetArcTaskIdMap().empty());
  EXPECT_TRUE(read_test_api.GetArcWindowIdMap().empty());
}

}  // namespace full_restore

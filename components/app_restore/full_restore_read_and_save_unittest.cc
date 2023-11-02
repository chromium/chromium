// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/app_types.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/test/test_views_delegate.h"
#include "url/gurl.h"

namespace full_restore {

namespace {

constexpr char kAppId[] = "aaa";

constexpr int32_t kId1 = 100;
constexpr int32_t kId2 = 200;
constexpr int32_t kId3 = 300;

constexpr int32_t kActivationIndex1 = 100;
constexpr int32_t kActivationIndex2 = 101;

constexpr int32_t kArcSessionId1 = 1;
constexpr int32_t kArcSessionId2 =
    app_restore::kArcSessionIdOffsetForRestoredLaunching + 1;

constexpr int32_t kArcTaskId1 = 666;
constexpr int32_t kArcTaskId2 = 888;

constexpr char kFilePath1[] = "path1";
constexpr char kFilePath2[] = "path2";

constexpr char kHandlerId[] = "audio";

constexpr char kExampleUrl1[] = "https://www.example1.com";
constexpr char kExampleUrl2[] = "https://www.example2.com";

constexpr char kLacrosWindowId[] = "123";

constexpr uint32_t kBrowserSessionId = 56;

}  // namespace

class FullRestoreReadHandlerTestApi {
 public:
  explicit FullRestoreReadHandlerTestApi(FullRestoreReadHandler* read_handler)
      : read_handler_(read_handler) {}

  FullRestoreReadHandlerTestApi(const FullRestoreReadHandlerTestApi&) = delete;
  FullRestoreReadHandlerTestApi& operator=(
      const FullRestoreReadHandlerTestApi&) = delete;
  ~FullRestoreReadHandlerTestApi() = default;

  const app_restore::ArcReadHandler* GetArcReadHander() const {
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

  void ClearRestoreData() {
    read_handler_->profile_path_to_restore_data_.clear();
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
    it->second.second = it->second.second - base::Seconds(601);
  }

  base::RepeatingTimer* GetArcCheckTimer() {
    return &arc_save_handler()->check_timer_;
  }

  void CheckArcTasks() { arc_save_handler()->CheckTasksForAppLaunching(); }

  const LacrosSaveHandler* GetLacrosSaveHander() const {
    DCHECK(save_handler_);
    return save_handler_->lacros_save_handler_.get();
  }

  const std::map<std::string, LacrosSaveHandler::WindowData>&
  GetLacrosWindowCandidates() const {
    const auto* lacros_save_handler = GetLacrosSaveHander();
    DCHECK(lacros_save_handler);
    return lacros_save_handler->window_candidates_;
  }

  const std::map<std::string, std::string>& GetLacrosWindowIdToAppIdMap()
      const {
    const auto* lacros_save_handler = GetLacrosSaveHander();
    DCHECK(lacros_save_handler);
    return lacros_save_handler->lacros_window_id_to_app_id_;
  }

  int32_t GetLacrosWindowId(std::string lacros_window_id) const {
    const auto& window_candidates = GetLacrosWindowCandidates();
    auto it = window_candidates.find(lacros_window_id);
    return it != window_candidates.end() ? it->second.window_id : -1;
  }

  void ClearRestoreData() {
    save_handler_->profile_path_to_restore_data_.clear();
  }

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
    scoped_feature_list_.InitAndEnableFeature(features::kFullRestoreForLacros);
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    aura_test_helper_.SetUp();
  }

  void TearDown() override {
    FullRestoreSaveHandler::GetInstance()->ClearForTesting();
  }

  const base::FilePath& GetPath() { return tmp_dir_.GetPath(); }

  void ReadFromFile(const base::FilePath& file_path, bool clear_data = true) {
    FullRestoreReadHandler* read_handler =
        FullRestoreReadHandler::GetInstance();
    if (clear_data)
      FullRestoreReadHandlerTestApi(read_handler).ClearRestoreData();

    base::RunLoop run_loop;

    read_handler->ReadFromFile(
        file_path,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<app_restore::RestoreData> restore_data) {
              run_loop.Quit();
              restore_data_ = std::move(restore_data);
            }));
    run_loop.Run();
  }

  FullRestoreSaveHandler* GetSaveHandler(bool start_save_timer = true) {
    auto* save_handler = FullRestoreSaveHandler::GetInstance();
    save_handler->SetActiveProfilePath(GetPath());
    save_handler->AllowSave();
    return save_handler;
  }

  const app_restore::RestoreData* GetRestoreData(
      const base::FilePath& file_path) {
    return restore_data_.get();
  }

  void AddAppLaunchInfo(const base::FilePath& file_path, int32_t id) {
    SaveAppLaunchInfo(file_path,
                      std::make_unique<app_restore::AppLaunchInfo>(kAppId, id));
  }

  void AddArcAppLaunchInfo(const base::FilePath& file_path) {
    SaveAppLaunchInfo(file_path, std::make_unique<app_restore::AppLaunchInfo>(
                                     kAppId, /*event_flags=*/0, kArcSessionId1,
                                     /*display_id*/ 0));
  }

  void AddBrowserLaunchInfo(const base::FilePath& file_path,
                            int32_t id,
                            std::vector<GURL> urls,
                            int32_t active_tab_index = 0) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_constants::kChromeAppId, id);
    launch_info->urls = urls;
    launch_info->active_tab_index = active_tab_index;
    SaveAppLaunchInfo(file_path, std::move(launch_info));
  }

  void AddChromeAppLaunchInfo(const base::FilePath& file_path) {
    auto app_launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        kAppId, kHandlerId,
        std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                    base::FilePath(kFilePath2)});
    app_launch_info->window_id = kId1;
    SaveAppLaunchInfo(file_path, std::move(app_launch_info));
  }

  std::unique_ptr<views::Widget> CreateLacrosWidget(
      const std::string& lacros_window_id,
      int32_t restore_session_id,
      int32_t restore_window_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);

    params.bounds = gfx::Rect(5, 5, 20, 20);
    params.context = aura_test_helper_.GetContext();
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    params.init_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::LACROS));
    params.init_properties_container.SetProperty(app_restore::kLacrosWindowId,
                                                 lacros_window_id);

    params.init_properties_container.SetProperty(app_restore::kWindowIdKey,
                                                 restore_session_id);
    params.init_properties_container.SetProperty(
        app_restore::kRestoreWindowIdKey, restore_window_id);

    auto widget = std::make_unique<views::Widget>();
    widget->Init(std::move(params));

    widget->Show();
    return widget;
  }

  std::unique_ptr<aura::Window> CreateLacrosWindow(
      const std::string& lacros_window_id,
      int32_t restore_session_id,
      int32_t restore_window_id) {
    auto window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::LACROS));
    window->SetProperty(app_restore::kLacrosWindowId,
                        std::string(kLacrosWindowId));
    window->SetProperty(app_restore::kWindowIdKey, restore_session_id);
    window->SetProperty(app_restore::kRestoreWindowIdKey, restore_window_id);
    return window;
  }

  void SaveWindowInfo(aura::Window* window, int32_t activation_index) {
    app_restore::WindowInfo window_info;
    window_info.window = window;
    window_info.activation_index = activation_index;
    full_restore::SaveWindowInfo(window_info);
  }

  std::unique_ptr<aura::Window> CreateWindowInfo(
      int32_t id,
      int32_t index,
      ash::AppType app_type = ash::AppType::BROWSER) {
    std::unique_ptr<aura::Window> window(
        aura::test::CreateTestWindowWithId(id, nullptr));
    app_restore::WindowInfo window_info;
    window_info.window = window.get();
    window->SetProperty(aura::client::kAppType, static_cast<int>(app_type));
    window->SetProperty(app_restore::kWindowIdKey, id);
    window_info.activation_index = index;
    full_restore::SaveWindowInfo(window_info);
    return window;
  }

  std::unique_ptr<app_restore::WindowInfo> GetArcWindowInfo(
      int32_t restore_window_id) {
    std::unique_ptr<aura::Window> window(
        aura::test::CreateTestWindowWithId(restore_window_id, nullptr));
    window->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::ARC_APP));
    window->SetProperty(app_restore::kRestoreWindowIdKey, restore_window_id);
    return FullRestoreReadHandler::GetInstance()->GetWindowInfo(window.get());
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

  std::unique_ptr<app_restore::RestoreData> restore_data_;

  views::TestViewsDelegate test_views_delegate_;
  aura::test::AuraTestHelper aura_test_helper_;
};

TEST_F(FullRestoreReadAndSaveTest, ReadEmptyRestoreData) {
  ReadFromFile(GetPath());
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  ASSERT_TRUE(restore_data->app_id_to_launch_list().empty());
}

TEST_F(FullRestoreReadAndSaveTest, StopSavingWhenShutdown) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add app launch info, and verify the timer starts.
  AddAppLaunchInfo(GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Simulate timeout.
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Add one more app launch info, and verify the timer is running.
  AddAppLaunchInfo(GetPath(), kId2);
  EXPECT_TRUE(timer->IsRunning());

  // Simulate shutdown.
  save_handler->SetShutDown();

  // Simulate timeout.
  timer->FireNow();
  task_environment().RunUntilIdle();

  FullRestoreReadHandler* read_handler = FullRestoreReadHandler::GetInstance();
  FullRestoreReadHandlerTestApi(read_handler).ClearRestoreData();

  // Add one more app launch info, to simulate a window is created during the
  // system startup phase.
  AddAppLaunchInfo(GetPath(), kId3);
  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath(), /*clear_data=*/false);

  // Verify the restore data can be read correctly.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);

  const auto& launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list.size());

  // Verify for |kAppId|.
  const auto launch_list_it = launch_list.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(1u, launch_list_it->second.size());

  // Verify the restore data for `kId1` exists.
  EXPECT_TRUE(base::Contains(launch_list_it->second, kId1));

  // Verify the restore data for `kId2` and `kId3` doesn't exist.
  EXPECT_FALSE(base::Contains(launch_list_it->second, kId2));
  EXPECT_FALSE(base::Contains(launch_list_it->second, kId3));
}

TEST_F(FullRestoreReadAndSaveTest, StartSaveTimer) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add app launch info, and verify the timer starts.
  AddAppLaunchInfo(GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Simulate timeout.
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Simulate the system reboots.
  FullRestoreReadHandler* read_handler = FullRestoreReadHandler::GetInstance();
  FullRestoreReadHandlerTestApi(read_handler).ClearRestoreData();
  save_handler->ClearForTesting();

  // Add one more app launch info, to simulate an app is launched during the
  // system startup phase.
  AddAppLaunchInfo(GetPath(), kId2);

  // Verify `timer` doesn't start.
  EXPECT_FALSE(timer->IsRunning());

  ReadFromFile(GetPath(), /*clear_data=*/false);

  // Verify the restore data can be read correctly.
  auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);

  auto& launch_list1 = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list1.size());

  // Verify for |kAppId|.
  auto launch_list_it = launch_list1.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list1.end());
  EXPECT_EQ(1u, launch_list_it->second.size());

  // Verify the restore data for `kId1` exists.
  EXPECT_TRUE(base::Contains(launch_list_it->second, kId1));

  // Verify the restore data for `kId2` doesn't exist.
  EXPECT_FALSE(base::Contains(launch_list_it->second, kId2));

  // Simulate the system reboots.
  FullRestoreReadHandlerTestApi(read_handler).ClearRestoreData();
  save_handler->ClearForTesting();

  ReadFromFile(GetPath(), /*clear_data=*/false);

  // Verify the original restore data can be read correctly.
  restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);

  auto& launch_list2 = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list2.size());

  // Verify for |kAppId|.
  launch_list_it = launch_list2.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list2.end());
  EXPECT_EQ(1u, launch_list_it->second.size());

  // Verify the restore data for `kId1` exists.
  EXPECT_TRUE(base::Contains(launch_list_it->second, kId1));

  // Verify the restore data for `kId2` doesn't exist.
  EXPECT_FALSE(base::Contains(launch_list_it->second, kId2));
}

TEST_F(FullRestoreReadAndSaveTest, SaveAndReadRestoreData) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add app launch info, and verify the timer starts.
  AddAppLaunchInfo(GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Add one more app launch info, and verify the timer is still running.
  AddAppLaunchInfo(GetPath(), kId2);
  EXPECT_TRUE(timer->IsRunning());

  std::unique_ptr<aura::Window> window1 =
      CreateWindowInfo(kId2, kActivationIndex2);

  // Simulate timeout, and verify the timer stops.
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Modify the window info, and verify the timer starts.
  std::unique_ptr<aura::Window> window2 =
      CreateWindowInfo(kId1, kActivationIndex1);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Verify that GetAppId() can get correct app id for |window1| and |window2|.
  EXPECT_EQ(save_handler->GetAppId(window1.get()), kAppId);
  EXPECT_EQ(save_handler->GetAppId(window2.get()), kAppId);

  // Modify the window id from `kId2` to `kId3` for `kAppId`.
  save_handler->ModifyWindowId(GetPath(), kAppId, kId2, kId3);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Verify now GetAppId() can still get correct id for |window1| whose
  // app_restore::kWindowIdKey has changed.
  EXPECT_EQ(save_handler->GetAppId(window1.get()), kAppId);

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

  // Verify the restore data for |kId2| doesn't exist.
  EXPECT_TRUE(!base::Contains(launch_list_it->second, kId2));

  // Verify the restore data for |kId2| is moved to |kId3|.
  const auto app_restore_data_it3 = launch_list_it->second.find(kId3);
  ASSERT_NE(app_restore_data_it3, launch_list_it->second.end());

  const auto& data3 = app_restore_data_it3->second;
  EXPECT_TRUE(data3->activation_index.has_value());
  EXPECT_EQ(kActivationIndex2, data3->activation_index.value());
}

TEST_F(FullRestoreReadAndSaveTest, MultipleFilePaths) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  base::ScopedTempDir tmp_dir1;
  base::ScopedTempDir tmp_dir2;
  ASSERT_TRUE(tmp_dir1.CreateUniqueTempDir());
  ASSERT_TRUE(tmp_dir2.CreateUniqueTempDir());

  save_handler->SetActiveProfilePath(tmp_dir1.GetPath());

  // Add app launch info for |tmp_dir1|, and verify the timer starts.
  AddAppLaunchInfo(tmp_dir1.GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Add app launch info for |tmp_dir2|, and verify the timer is still running.
  AddAppLaunchInfo(tmp_dir2.GetPath(), kId2);
  EXPECT_TRUE(timer->IsRunning());

  CreateWindowInfo(kId2, kActivationIndex2);

  // Simulate timeout, and verify the timer stops.
  timer->FireNow();
  task_environment().RunUntilIdle();
  EXPECT_FALSE(timer->IsRunning());

  // Modify the window info, and verify the timer starts.
  CreateWindowInfo(kId1, kActivationIndex1);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  VerifyRestoreData(tmp_dir1.GetPath(), kId1, kActivationIndex1);

  // Set the active profile path to `tmp_dir2` to simulate the user is switched.
  save_handler->SetActiveProfilePath(tmp_dir2.GetPath());
  timer->FireNow();
  task_environment().RunUntilIdle();

  VerifyRestoreData(tmp_dir2.GetPath(), kId2, kActivationIndex2);
}

TEST_F(FullRestoreReadAndSaveTest, ClearRestoreData) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add app launch info, and verify the timer starts.
  AddAppLaunchInfo(GetPath(), kId1);
  EXPECT_TRUE(timer->IsRunning());

  // Simulate timeout.
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Read the restore data.
  ReadFromFile(GetPath());

  // Clear restore data to simulate the system reboot.
  test_api.ClearRestoreData();

  // Verify the restore data can be read correctly.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);

  const auto& launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list.size());

  // Verify for `kAppId`.
  const auto launch_list_it = launch_list.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(1u, launch_list_it->second.size());

  // Verify for `kId1`.
  const auto app_restore_data_it1 = launch_list_it->second.find(kId1);
  EXPECT_TRUE(app_restore_data_it1 != launch_list_it->second.end());

  // Simulate timeout to clear restore data.
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Read the restore data.
  ReadFromFile(GetPath());

  // Verify the restore data has been cleared.
  ASSERT_TRUE(GetRestoreData(GetPath()));
  ASSERT_TRUE(GetRestoreData(GetPath())->app_id_to_launch_list().empty());
}

TEST_F(FullRestoreReadAndSaveTest, ArcWindowSaving) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
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

  // Create a window to associate with the task id.
  std::unique_ptr<aura::Window> window =
      CreateWindowInfo(kArcTaskId1, kActivationIndex1, ash::AppType::ARC_APP);
  // Test that using ARC task id we can get the correct app id for the window.
  EXPECT_EQ(save_handler->GetAppId(window.get()), kAppId);

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
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
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
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
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
  // The following is necessary for making `ShouldUseFullRestoreArcData()` and
  // `read_handler->IsFullRestoreRunning()` return true;
  read_handler->SetActiveProfilePath(GetPath());
  read_handler->SetStartTimeForProfile(GetPath());

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
  const std::unique_ptr<app_restore::AppRestoreData>& data =
      app_restore_data_it->second;
  EXPECT_TRUE(data->activation_index.has_value());
  EXPECT_EQ(kActivationIndex1, data->activation_index.value());

  // Simulate the ARC app launching, and set the arc session id kArcSessionId2
  // for the restore window id |kArcTaskId1|.
  read_handler->SetArcSessionIdForWindowId(kArcSessionId2, kArcTaskId1);
  EXPECT_EQ(1u, read_test_api.GetArcSessionIdMap().size());

  // Before OnTaskCreated is called, return |kArcTaskId1| for |kArcSessionId2|
  // to simulate the ghost window property setting.
  EXPECT_EQ(kArcTaskId1,
            app_restore::GetArcRestoreWindowIdForSessionId(kArcSessionId2));

  // Before OnTaskCreated is called, return -1 to add the ARC app window to the
  // hidden container.
  EXPECT_EQ(app_restore::kParentToHiddenContainer,
            app_restore::GetArcRestoreWindowIdForTaskId(kArcTaskId2));

  // Call OnTaskCreated to simulate that the ARC app with |kAppId| has been
  // launched, and the new task id |kArcTaskId2| has been created with
  // |kArcSessionId2| returned.
  read_handler->OnTaskCreated(kAppId, kArcTaskId2, kArcSessionId2);
  EXPECT_EQ(1u, read_test_api.GetArcTaskIdMap().size());

  // Since we have got the new task with |kArcSessionId2|, the arc session id
  // map can be cleared. And verify that we can get the restore window id
  // |kArcTaskId1| with the new |kArcTaskId2|.
  EXPECT_TRUE(read_test_api.GetArcSessionIdMap().empty());
  EXPECT_EQ(kArcTaskId1,
            app_restore::GetArcRestoreWindowIdForTaskId(kArcTaskId2));

  // Verify |window_info| for |kArcTaskId1|.
  auto window_info = GetArcWindowInfo(kArcTaskId1);
  EXPECT_TRUE(window_info);
  EXPECT_EQ(kActivationIndex1, window_info->activation_index);

  // Call OnTaskDestroyed to simulate the ARC app launching has been finished
  // for |kArcTaskId2|, and verify the task id map is now empty and a invalid
  // value is returned when trying to get the restore window id.
  read_handler->OnTaskDestroyed(kArcTaskId2);
  EXPECT_EQ(0, app_restore::GetArcRestoreWindowIdForTaskId(kArcTaskId2));
  EXPECT_TRUE(read_test_api.GetArcTaskIdMap().empty());
  EXPECT_TRUE(read_test_api.GetArcWindowIdMap().empty());
}

TEST_F(FullRestoreReadAndSaveTest, ReadBrowserRestoreData) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add browser launch info.
  std::vector<GURL> urls = {GURL(kExampleUrl1), GURL(kExampleUrl2)};
  const int active_tab_index = 1;
  AddBrowserLaunchInfo(GetPath(), kId1, urls,
                       /*active_tab_index=*/active_tab_index);
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Now read from the file.
  ReadFromFile(GetPath());

  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  const auto& launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list.size());
  const auto launch_list_it = launch_list.find(app_constants::kChromeAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(1u, launch_list_it->second.size());
  const auto app_restore_data_it = launch_list_it->second.find(kId1);
  EXPECT_TRUE(app_restore_data_it != launch_list_it->second.end());

  const auto& data = app_restore_data_it->second;
  EXPECT_TRUE(data->urls.has_value());
  EXPECT_EQ(data->urls.value().size(), 2u);
  EXPECT_EQ(data->urls.value()[0], GURL(kExampleUrl1));
  EXPECT_EQ(data->urls.value()[1], GURL(kExampleUrl2));

  EXPECT_TRUE(data->active_tab_index.has_value());
  EXPECT_EQ(data->active_tab_index.value(), active_tab_index);
}

TEST_F(FullRestoreReadAndSaveTest, ReadChromeAppRestoreData) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add Chrome app launch info.
  AddChromeAppLaunchInfo(GetPath());
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  task_environment().RunUntilIdle();

  // Now read from the file.
  ReadFromFile(GetPath());

  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  const auto& launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, launch_list.size());
  const auto launch_list_it = launch_list.find(kAppId);
  EXPECT_TRUE(launch_list_it != launch_list.end());
  EXPECT_EQ(1u, launch_list_it->second.size());
  const auto app_restore_data_it = launch_list_it->second.find(kId1);
  EXPECT_TRUE(app_restore_data_it != launch_list_it->second.end());

  const auto& data = app_restore_data_it->second;
  EXPECT_TRUE(data->handler_id.has_value());
  EXPECT_EQ(kHandlerId, data->handler_id.value());
  EXPECT_TRUE(data->file_paths.has_value());
  EXPECT_EQ(2u, data->file_paths.value().size());
  EXPECT_EQ(base::FilePath(kFilePath1), data->file_paths.value()[0]);
  EXPECT_EQ(base::FilePath(kFilePath2), data->file_paths.value()[1]);
}

// Verify the Lacros browser window is saved correctly when the window is
// created first, then OnLacrosWindowAdded is called.
TEST_F(FullRestoreReadAndSaveTest, LacrosBrowserWindowSavingCreateWindowFirst) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  const LacrosSaveHandler* lacros_save_handler = test_api.GetLacrosSaveHander();
  ASSERT_TRUE(lacros_save_handler);

  // Create a browser window first, then OnLacrosWindowAdded is called later.
  auto widget = CreateLacrosWidget(kLacrosWindowId, kBrowserSessionId,
                                   /*restored_browser_session_id=*/0);
  auto* window = widget->GetNativeWindow();
  SaveWindowInfo(window, kActivationIndex1);

  // Verify the browser window is saved.
  EXPECT_EQ(app_constants::kLacrosAppId,
            save_handler->GetAppId(widget->GetNativeWindow()));
  auto window_info = save_handler->GetWindowInfo(
      GetPath(), app_constants::kLacrosAppId, kBrowserSessionId);
  EXPECT_EQ(kActivationIndex1, window_info->activation_index.value());

  // Modify the window info.
  SaveWindowInfo(window, kActivationIndex2);
  window_info = save_handler->GetWindowInfo(
      GetPath(), app_constants::kLacrosAppId, kBrowserSessionId);
  EXPECT_EQ(kActivationIndex2, window_info->activation_index.value());

  widget.reset();
  ASSERT_FALSE(save_handler->GetWindowInfo(
      GetPath(), app_constants::kLacrosAppId, kBrowserSessionId));

  timer->FireNow();
  // Wait for the restore data to be written to the full restore file.
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify there is not restore data.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  EXPECT_TRUE(restore_data->app_id_to_launch_list().empty());
}

// Verify the Lacros browser window is saved correctly when
// OnLacrosWindowAdded is called first, then the window is init later.
TEST_F(FullRestoreReadAndSaveTest,
       LacrosBrowserWindowSavingOnLacrosWindowAddedCalledFirst) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();
  const LacrosSaveHandler* lacros_save_handler = test_api.GetLacrosSaveHander();
  ASSERT_TRUE(lacros_save_handler);

  // OnLacrosWindowAdded is called first, then init the browser window later.
  auto window = CreateLacrosWindow(kLacrosWindowId, kBrowserSessionId,
                                   /*restored_browser_session_id=*/0);
  window->Init(ui::LAYER_NOT_DRAWN);

  SaveWindowInfo(window.get(), kActivationIndex1);

  // Verify the browser window is saved.
  EXPECT_EQ(app_constants::kLacrosAppId, save_handler->GetAppId(window.get()));
  auto window_info = save_handler->GetWindowInfo(
      GetPath(), app_constants::kLacrosAppId, kBrowserSessionId);
  EXPECT_EQ(kActivationIndex1, window_info->activation_index.value());

  // Modify the window info.
  SaveWindowInfo(window.get(), kActivationIndex2);
  window_info = save_handler->GetWindowInfo(
      GetPath(), app_constants::kLacrosAppId, kBrowserSessionId);
  EXPECT_EQ(kActivationIndex2, window_info->activation_index.value());

  window.reset();

  // Wait for `save_handler` to fresh the full restore file.
  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify there is not restore data.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  EXPECT_TRUE(restore_data->app_id_to_launch_list().empty());
}

// Verify the Lacros Chrome app window is saved correctly when the window is
// created first, then OnAppWindowAdded is called.
TEST_F(FullRestoreReadAndSaveTest,
       LacrosChromeAppWindowSavingCreateWindowFirst) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add a Chrome app launch info.
  SaveAppLaunchInfo(
      GetPath(), std::make_unique<app_restore::AppLaunchInfo>(
                     kAppId, apps::LaunchContainer::kLaunchContainerNone,
                     WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
                     std::vector<base::FilePath>{}, nullptr));
  const LacrosSaveHandler* lacros_save_handler = test_api.GetLacrosSaveHander();
  ASSERT_TRUE(lacros_save_handler);

  // Create a Chrome app window first, then the crosapi OnAppWindowAdded is
  // called later.
  auto widget = CreateLacrosWidget(kLacrosWindowId, kBrowserSessionId,
                                   /*restored_browser_session_id=*/0);
  auto* window = widget->GetNativeWindow();
  EXPECT_FALSE(test_api.GetLacrosWindowCandidates().empty());
  SaveWindowInfo(window, kActivationIndex1);
  OnLacrosChromeAppWindowAdded(kAppId, kLacrosWindowId);

  // Verify the Chrome app window is saved.
  EXPECT_TRUE(test_api.GetLacrosWindowIdToAppIdMap().empty());
  EXPECT_EQ(save_handler->GetAppId(widget->GetNativeWindow()), kAppId);
  auto window_info = save_handler->GetWindowInfo(
      GetPath(), kAppId, test_api.GetLacrosWindowId(kLacrosWindowId));
  EXPECT_EQ(kActivationIndex1, window_info->activation_index.value());

  // Modify the window info.
  SaveWindowInfo(window, kActivationIndex2);
  window_info = save_handler->GetWindowInfo(
      GetPath(), kAppId, test_api.GetLacrosWindowId(kLacrosWindowId));
  EXPECT_EQ(kActivationIndex2, window_info->activation_index.value());

  // Destroy the window first, then call the crosapi OnAppWindowRemoved.
  widget.reset();
  OnLacrosChromeAppWindowRemoved(kAppId, kLacrosWindowId);
  EXPECT_TRUE(test_api.GetLacrosWindowCandidates().empty());
  EXPECT_TRUE(test_api.GetLacrosWindowIdToAppIdMap().empty());

  // Wait for `save_handler` to fresh the full restore file.
  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify there is not restore data.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  EXPECT_TRUE(restore_data->app_id_to_launch_list().empty());
}

// Verify the Lacros Chrome app window is saved correctly when OnAppWindowAdded
// is called first, then the window is created later.
TEST_F(FullRestoreReadAndSaveTest,
       LacrosChromeAppWindowSavingOnAppWindowCalledFirst) {
  FullRestoreSaveHandler* save_handler = GetSaveHandler();
  FullRestoreSaveHandlerTestApi test_api(save_handler);

  save_handler->SetPrimaryProfilePath(GetPath());
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();

  // Add a Chrome app launch info.
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
  intent->activity_name = "activity_name";
  SaveAppLaunchInfo(
      GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          kAppId, apps::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::CURRENT_TAB, display::kInvalidDisplayId,
          std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                      base::FilePath(kFilePath2)},
          std::move(intent)));
  const LacrosSaveHandler* lacros_save_handler = test_api.GetLacrosSaveHander();
  ASSERT_TRUE(lacros_save_handler);

  // The crosapi OnAppWindowAdded is called first, then create a Chrome app
  // window later.
  OnLacrosChromeAppWindowAdded(kAppId, kLacrosWindowId);
  EXPECT_FALSE(test_api.GetLacrosWindowIdToAppIdMap().empty());
  auto widget = CreateLacrosWidget(kLacrosWindowId, kBrowserSessionId,
                                   /*restored_browser_session_id=*/0);
  auto* window = widget->GetNativeWindow();
  EXPECT_FALSE(test_api.GetLacrosWindowCandidates().empty());
  SaveWindowInfo(window, kActivationIndex1);

  // Verify the Chrome app window is saved.
  EXPECT_EQ(save_handler->GetAppId(widget->GetNativeWindow()), kAppId);
  auto window_info = save_handler->GetWindowInfo(
      GetPath(), kAppId, test_api.GetLacrosWindowId(kLacrosWindowId));
  EXPECT_EQ(kActivationIndex1, window_info->activation_index.value());

  // Modify the window info.
  SaveWindowInfo(window, kActivationIndex2);
  window_info = save_handler->GetWindowInfo(
      GetPath(), kAppId, test_api.GetLacrosWindowId(kLacrosWindowId));
  EXPECT_EQ(kActivationIndex2, window_info->activation_index.value());

  // Call the crosapi OnAppWindowRemoved first, then destroy the window.
  OnLacrosChromeAppWindowRemoved(kAppId, kLacrosWindowId);
  widget.reset();
  EXPECT_TRUE(test_api.GetLacrosWindowCandidates().empty());
  EXPECT_TRUE(test_api.GetLacrosWindowIdToAppIdMap().empty());

  timer->FireNow();
  task_environment().RunUntilIdle();

  ReadFromFile(GetPath());

  // Verify there is not restore data.
  const auto* restore_data = GetRestoreData(GetPath());
  ASSERT_TRUE(restore_data);
  EXPECT_TRUE(restore_data->app_id_to_launch_list().empty());
}

}  // namespace full_restore

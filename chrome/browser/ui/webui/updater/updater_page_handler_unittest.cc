// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/updater/updater_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Field;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {

auto IsUpdaterState(std::string_view active_version,
                    const std::vector<std::string>& inactive_versions,
                    std::optional<base::Time> last_checked,
                    std::optional<base::Time> last_started,
                    const base::FilePath& installation_directory,
                    std::string_view policies) {
  using updater_ui::mojom::UpdaterState;
  return Pointee(AllOf(
      Field(&UpdaterState::active_version, active_version),
      Field(&UpdaterState::inactive_versions,
            ElementsAreArray(inactive_versions)),
      Field(&UpdaterState::last_checked, last_checked),
      Field(&UpdaterState::last_started, last_started),
      Field(&UpdaterState::installation_directory, installation_directory),
      Field(&UpdaterState::policies, policies)));
}

auto IsAppState(std::string_view app_id,
                std::string_view version,
                std::optional<std::string_view> cohort) {
  using updater_ui::mojom::AppState;
  return Pointee(AllOf(Field(&AppState::app_id, app_id),
                       Field(&AppState::version, version),
                       Field(&AppState::cohort, cohort)));
}

auto IsEnterpriseCompanionState(std::string_view version,
                                const base::FilePath& installation_directory) {
  using updater_ui::mojom::EnterpriseCompanionState;
  return Pointee(AllOf(Field(&EnterpriseCompanionState::version, version),
                       Field(&EnterpriseCompanionState::installation_directory,
                             installation_directory)));
}

class MockUpdaterPageHandlerDelegate : public UpdaterPageHandler::Delegate {
 public:
  MOCK_METHOD(std::optional<base::FilePath>,
              GetUpdaterInstallDirectory,
              (updater::UpdaterScope scope),
              (const, override));
  MOCK_METHOD(std::optional<base::FilePath>,
              GetEnterpriseCompanionInstallDirectory,
              (),
              (const, override));
  MOCK_METHOD(
      void,
      GetSystemUpdaterState,
      (base::OnceCallback<void(const updater::mojom::UpdaterState&)> callback),
      (const, override));
  MOCK_METHOD(
      void,
      GetUserUpdaterState,
      (base::OnceCallback<void(const updater::mojom::UpdaterState&)> callback),
      (const, override));
  MOCK_METHOD(void,
              GetSystemPoliciesJson,
              (base::OnceCallback<void(const std::string&)> callback),
              (const, override));
  MOCK_METHOD(void,
              GetUserPoliciesJson,
              (base::OnceCallback<void(const std::string&)> callback),
              (const, override));
  MOCK_METHOD(void,
              GetSystemUpdaterAppStates,
              (base::OnceCallback<
                  void(const std::vector<updater::mojom::AppState>&)> callback),
              (const, override));
  MOCK_METHOD(void,
              GetUserUpdaterAppStates,
              (base::OnceCallback<
                  void(const std::vector<updater::mojom::AppState>&)> callback),
              (const, override));

 private:
  ~MockUpdaterPageHandlerDelegate() override = default;
};

class MockUpdaterPage : public updater_ui::mojom::Page {
 public:
  mojo::PendingRemote<updater_ui::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<updater_ui::mojom::Page> receiver_{this};
};

class UpdaterPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<UpdaterPageHandler>(
        /*profile=*/nullptr,
        mojo::PendingReceiver<updater_ui::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), mock_delegate_);
  }

 protected:
  base::test::TaskEnvironment environment_;
  MockUpdaterPage mock_page_;
  std::unique_ptr<UpdaterPageHandler> handler_;
  scoped_refptr<MockUpdaterPageHandlerDelegate> mock_delegate_ =
      base::MakeRefCounted<MockUpdaterPageHandlerDelegate>();
};

TEST_F(UpdaterPageHandlerTest, GetAllUpdaterEvents_OneScope) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_file =
      temp_dir.GetPath().AppendASCII("updater_history.jsonl");
  ASSERT_TRUE(base::WriteFile(log_file, "event1\nevent2\n"));

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillOnce(Return(temp_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillOnce(Return(std::nullopt));

  base::RunLoop run_loop;
  handler_->GetAllUpdaterEvents(
      base::BindLambdaForTesting([&](const std::vector<std::string>& events) {
        EXPECT_THAT(events, ElementsAre("event1", "event2"));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetAllUpdaterEvents_BothScopes) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_file =
      temp_dir.GetPath().AppendASCII("updater_history.jsonl");
  ASSERT_TRUE(base::WriteFile(log_file, "event1\n"));
  base::FilePath old_log_file =
      temp_dir.GetPath().AppendASCII("updater_history.jsonl.old");
  ASSERT_TRUE(base::WriteFile(old_log_file, "event2\n"));

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillOnce(Return(temp_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillOnce(Return(std::nullopt));

  base::RunLoop run_loop;
  handler_->GetAllUpdaterEvents(
      base::BindLambdaForTesting([&](const std::vector<std::string>& events) {
        EXPECT_THAT(events, UnorderedElementsAre("event1", "event2"));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetAllUpdaterEvents_NoFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillOnce(Return(temp_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillOnce(Return(std::nullopt));

  base::RunLoop run_loop;
  handler_->GetAllUpdaterEvents(
      base::BindLambdaForTesting([&](const std::vector<std::string>& events) {
        EXPECT_TRUE(events.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetAllUpdaterEvents_MissingDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillOnce(
          Return(base::FilePath(temp_dir.GetPath().AppendASCII("missing"))));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillOnce(Return(std::nullopt));

  base::RunLoop run_loop;
  handler_->GetAllUpdaterEvents(
      base::BindLambdaForTesting([&](const std::vector<std::string>& events) {
        EXPECT_TRUE(events.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetUpdaterStates_BothScopesSuccess) {
  base::ScopedTempDir system_dir;
  ASSERT_TRUE(system_dir.CreateUniqueTempDir());
  base::ScopedTempDir user_dir;
  ASSERT_TRUE(user_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(system_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(user_dir.GetPath()));

  updater::mojom::UpdaterState system_state(
      /*active_version=*/"1.2.3.4",
      /*inactive_versions=*/{"1.0.0.0"},
      /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(1000),
      /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(2000));
  updater::mojom::UpdaterState user_state(
      /*active_version=*/"2.3.4.5",
      /*inactive_versions=*/{"2.0.0.0"},
      /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(3000),
      /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(4000));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_))
      .WillOnce(RunOnceCallback<0>(system_state));
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterState(_))
      .WillOnce(RunOnceCallback<0>(user_state));
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_))
      .WillOnce(RunOnceCallback<0>("system policy"));
  EXPECT_CALL(*mock_delegate_, GetUserPoliciesJson(_))
      .WillOnce(RunOnceCallback<0>("user policy"));

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetUpdaterStatesResponsePtr response,
            std::move(result));

        EXPECT_THAT(
            response->system,
            IsUpdaterState(
                /*active_version=*/"1.2.3.4",
                /*inactive_versions=*/{"1.0.0.0"},
                /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(1000),
                /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(2000),
                /*installation_directory=*/system_dir.GetPath(),
                /*policies=*/"system policy"));
        EXPECT_THAT(
            response->user,
            IsUpdaterState(
                /*active_version=*/"2.3.4.5",
                /*inactive_versions=*/{"2.0.0.0"},
                /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(3000),
                /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(4000),
                /*installation_directory=*/user_dir.GetPath(),
                /*policies=*/"user policy"));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetUpdaterStates_SystemPathMissing) {
  base::ScopedTempDir user_dir;
  ASSERT_TRUE(user_dir.CreateUniqueTempDir());
  base::FilePath system_dir = user_dir.GetPath().AppendASCII("missing");

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(system_dir));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(user_dir.GetPath()));

  updater::mojom::UpdaterState user_state(
      /*active_version=*/"2.3.4.5",
      /*inactive_versions=*/{"2.0.0.0"},
      /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(3000),
      /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(4000));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterState(_))
      .WillOnce(RunOnceCallback<0>(user_state));
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetUserPoliciesJson(_))
      .WillOnce(RunOnceCallback<0>("user policy"));

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetUpdaterStatesResponsePtr response,
            std::move(result));

        EXPECT_FALSE(response->system);
        EXPECT_THAT(
            response->user,
            IsUpdaterState(
                /*active_version=*/"2.3.4.5",
                /*inactive_versions=*/{"2.0.0.0"},
                /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(3000),
                /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(4000),
                /*installation_directory=*/user_dir.GetPath(),
                /*policies=*/"user policy"));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetUpdaterStates_UserPathMissing) {
  base::ScopedTempDir system_dir;
  ASSERT_TRUE(system_dir.CreateUniqueTempDir());
  base::FilePath user_dir = system_dir.GetPath().AppendASCII("missing");

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(system_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(user_dir));

  updater::mojom::UpdaterState system_state(
      /*active_version=*/"1.2.3.4",
      /*inactive_versions=*/{"1.0.0.0"},
      /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(1000),
      /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(2000));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_))
      .WillOnce(RunOnceCallback<0>(system_state));
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterState(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_))
      .WillOnce(RunOnceCallback<0>("system policy"));
  EXPECT_CALL(*mock_delegate_, GetUserPoliciesJson(_)).Times(0);

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetUpdaterStatesResponsePtr response,
            std::move(result));

        EXPECT_THAT(
            response->system,
            IsUpdaterState(
                /*active_version=*/"1.2.3.4",
                /*inactive_versions=*/{"1.0.0.0"},
                /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(1000),
                /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(2000),
                /*installation_directory=*/system_dir.GetPath(),
                /*policies=*/"system policy"));
        EXPECT_FALSE(response->user);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetUpdaterStates_GetSystemInstallDirectoryFail) {
  base::ScopedTempDir system_dir;
  ASSERT_TRUE(system_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(system_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(std::nullopt));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterState(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetUserPoliciesJson(_)).Times(0);

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        EXPECT_FALSE(result.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetUpdaterStates_GetUserInstallDirectoryFail) {
  base::ScopedTempDir user_dir;
  ASSERT_TRUE(user_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(user_dir.GetPath()));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterState(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, GetUserPoliciesJson(_)).Times(0);

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        EXPECT_FALSE(result.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// If the updater responds with a default updater::mojom::UpdaterState instance,
// UpdaterPageHandler should not present an updater state.
TEST_F(UpdaterPageHandlerTest,
       GetUpdaterStates_DefaultUpdaterStateBecomesNull) {
  base::ScopedTempDir system_dir;
  ASSERT_TRUE(system_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(system_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(system_dir.GetPath().AppendASCII("missing")));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_))
      .WillOnce(RunOnceCallback<0>(updater::mojom::UpdaterState()));
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_))
      .WillOnce(RunOnceCallback<0>("system policy"));

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetUpdaterStatesResponsePtr response,
            std::move(result));

        EXPECT_FALSE(response->system);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// If the updater responds with an empty string for its policy set,
// UpdaterPageHandler should not present an updater state.
TEST_F(UpdaterPageHandlerTest, GetUpdaterStates_EmptyPolicyJsonBecomesNull) {
  base::ScopedTempDir system_dir;
  ASSERT_TRUE(system_dir.CreateUniqueTempDir());

  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem))
      .WillRepeatedly(Return(system_dir.GetPath()));
  EXPECT_CALL(*mock_delegate_,
              GetUpdaterInstallDirectory(updater::UpdaterScope::kUser))
      .WillRepeatedly(Return(system_dir.GetPath().AppendASCII("missing")));

  updater::mojom::UpdaterState system_state(
      /*active_version=*/"1.2.3.4",
      /*inactive_versions=*/{"1.0.0.0"},
      /*last_checked=*/base::Time::FromSecondsSinceUnixEpoch(1000),
      /*last_started=*/base::Time::FromSecondsSinceUnixEpoch(2000));

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterState(_))
      .WillOnce(RunOnceCallback<0>(system_state));
  EXPECT_CALL(*mock_delegate_, GetSystemPoliciesJson(_))
      .WillOnce(RunOnceCallback<0>(""));

  base::RunLoop run_loop;
  handler_->GetUpdaterStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetUpdaterStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetUpdaterStatesResponsePtr response,
            std::move(result));

        EXPECT_FALSE(response->system);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetAppStates_Success) {
  updater::mojom::AppState system_app;
  system_app.app_id = "system_app";
  system_app.version = "1.0";
  system_app.cohort = "system_cohort";

  updater::mojom::AppState user_app1;
  user_app1.app_id = "user_app1";
  user_app1.version = "2.0";
  user_app1.cohort = std::nullopt;

  updater::mojom::AppState user_app2;
  user_app2.app_id = "user_app2";
  user_app2.version = "3.0";
  user_app2.cohort = "";

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(
          std::vector<updater::mojom::AppState>{system_app}));
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(
          std::vector<updater::mojom::AppState>{user_app1, user_app2}));

  base::RunLoop run_loop;
  handler_->GetAppStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetAppStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetAppStatesResponsePtr response,
            std::move(result));
        EXPECT_THAT(
            response->system_apps,
            ElementsAre(IsAppState("system_app", "1.0", "system_cohort")));
        EXPECT_THAT(response->user_apps,
                    ElementsAre(IsAppState("user_app1", "2.0", std::nullopt),
                                IsAppState("user_app2", "3.0", std::nullopt)));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetAppStates_Empty) {
  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(std::vector<updater::mojom::AppState>{}));
  EXPECT_CALL(*mock_delegate_, GetUserUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(std::vector<updater::mojom::AppState>{}));

  base::RunLoop run_loop;
  handler_->GetAppStates(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetAppStatesResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetAppStatesResponsePtr response,
            std::move(result));
        EXPECT_TRUE(response->system_apps.empty());
        EXPECT_TRUE(response->user_apps.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetEnterpriseCompanionState_Success) {
  updater::mojom::AppState companion_app;
  companion_app.app_id = enterprise_companion::kCompanionAppId;
  companion_app.version = "1.0.0.0";

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(
          std::vector<updater::mojom::AppState>{companion_app}));
  EXPECT_CALL(*mock_delegate_, GetEnterpriseCompanionInstallDirectory())
      .WillOnce(
          Return(base::FilePath(FILE_PATH_LITERAL("/path/to/companion"))));

  base::RunLoop run_loop;
  handler_->GetEnterpriseCompanionState(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetEnterpriseCompanionStateResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetEnterpriseCompanionStateResponsePtr response,
            std::move(result));
        EXPECT_THAT(
            response->state,
            IsEnterpriseCompanionState(
                "1.0.0.0",
                base::FilePath(FILE_PATH_LITERAL("/path/to/companion"))));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetEnterpriseCompanionState_NotFound) {
  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(std::vector<updater::mojom::AppState>{}));

  base::RunLoop run_loop;
  handler_->GetEnterpriseCompanionState(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetEnterpriseCompanionStateResult result) {
        ASSERT_OK_AND_ASSIGN(
            updater_ui::mojom::GetEnterpriseCompanionStateResponsePtr response,
            std::move(result));
        EXPECT_FALSE(response->state);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(UpdaterPageHandlerTest, GetEnterpriseCompanionState_InstallDirMissing) {
  updater::mojom::AppState companion_app;
  companion_app.app_id = enterprise_companion::kCompanionAppId;
  companion_app.version = "1.0.0.0";

  EXPECT_CALL(*mock_delegate_, GetSystemUpdaterAppStates(_))
      .WillOnce(RunOnceCallback<0>(
          std::vector<updater::mojom::AppState>{companion_app}));
  EXPECT_CALL(*mock_delegate_, GetEnterpriseCompanionInstallDirectory())
      .WillOnce(Return(std::nullopt));

  base::RunLoop run_loop;
  handler_->GetEnterpriseCompanionState(base::BindLambdaForTesting(
      [&](UpdaterPageHandler::GetEnterpriseCompanionStateResult result) {
        EXPECT_FALSE(result.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace

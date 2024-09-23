// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/keystone/ksadmin.h"

#include <map>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/updater/app/server/posix/update_service_stub.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

// Returns the KSAdmin exit code, and sets `std_out` to the contents of its
// stdout.
int RunKSAdmin(std::string* std_out, const std::vector<std::string>& args) {
  base::FilePath out_dir;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  base::CommandLine command(out_dir.Append(FILE_PATH_LITERAL("ksadmin_test")));
  for (const auto& arg : args) {
    command.AppendArg(arg);
  }
  int exit_code = -1;
  base::GetAppOutputWithExitCode(command, std_out, &exit_code);
  return exit_code;
}

}  // namespace

TEST(KSAdminTest, ExitsOK) {
  std::string out;
  ASSERT_EQ(RunKSAdmin(&out, {}), 0);
  ASSERT_EQ(RunKSAdmin(&out, {"-H"}), 0);
  ASSERT_EQ(RunKSAdmin(&out, {"--unrecognized-argument", "value"}), 0);
}

TEST(KSAdminTest, PrintVersion) {
  std::string out;
  ASSERT_EQ(RunKSAdmin(&out, {"--ksadmin-version"}), 0);
  ASSERT_EQ(out, base::StrCat({kUpdaterVersion, "\n"}));
  out.clear();
  ASSERT_EQ(RunKSAdmin(&out, {"-k"}), 0);
  ASSERT_EQ(out, base::StrCat({kUpdaterVersion, "\n"}));
}

TEST(KSAdminTest, ParseCommandLine) {
  static const char* argv[] = {"ksadmin",
                               "--register",
                               "-P",
                               "com.google.kipple",
                               "-v=1.2.3.4",
                               "--xcpath",
                               "/Applications/GoogleKipple.app",
                               "--tag=abcd"};

  std::map<std::string, std::string> arg_map =
      ParseCommandLine(std::size(argv), argv);
  EXPECT_EQ(arg_map.size(), size_t{5});
  EXPECT_EQ(arg_map.count("register"), size_t{1});
  EXPECT_EQ(arg_map["register"], "");
  EXPECT_EQ(arg_map["P"], "com.google.kipple");
  EXPECT_EQ(arg_map["v"], "1.2.3.4");
  EXPECT_EQ(arg_map["xcpath"], "/Applications/GoogleKipple.app");
  EXPECT_EQ(arg_map["tag"], "abcd");
}

TEST(KSAdminTest, ParseCommandLine_DiffByCase) {
  const char* argv[] = {"ksadmin", "-k", "-K", "Tag"};

  std::map<std::string, std::string> arg_map =
      ParseCommandLine(std::size(argv), argv);
  EXPECT_EQ(arg_map.size(), size_t{2});
  EXPECT_EQ(arg_map.count("k"), size_t{1});
  EXPECT_EQ(arg_map["k"], "");
  EXPECT_EQ(arg_map["K"], "Tag");
}

TEST(KSAdminTest, ParseCommandLine_CombinedShortOptions) {
  const char* argv[] = {"ksadmin", "-pP", "com.google.Chrome", "-Uv=1.2.3.4"};

  std::map<std::string, std::string> arg_map =
      ParseCommandLine(std::size(argv), argv);
  EXPECT_EQ(arg_map.size(), size_t{4});
  EXPECT_EQ(arg_map.count("p"), size_t{1});
  EXPECT_EQ(arg_map["p"], "");
  EXPECT_EQ(arg_map["P"], "com.google.Chrome");
  EXPECT_EQ(arg_map["U"], "");
  EXPECT_EQ(arg_map["v"], "1.2.3.4");
}

TEST(KSAdminTest, Register) {
  if (GetUpdaterScopeForTesting() == UpdaterScope::kSystem) {
    return;
  }
  class MockUpdateService final : public UpdateService {
   public:
    MOCK_METHOD(void,
                GetVersion,
                (base::OnceCallback<void(const base::Version&)> callback),
                (override));
    MOCK_METHOD(void,
                FetchPolicies,
                (base::OnceCallback<void(int)> callback),
                (override));
    MOCK_METHOD(void,
                RegisterApp,
                (const RegistrationRequest& request,
                 base::OnceCallback<void(int)> callback),
                (override));
    MOCK_METHOD(
        void,
        GetAppStates,
        (base::OnceCallback<void(const std::vector<AppState>&)> callback),
        (override));
    MOCK_METHOD(void,
                RunPeriodicTasks,
                (base::OnceClosure callback),
                (override));
    MOCK_METHOD(void,
                CheckForUpdate,
                (const std::string& app_id,
                 Priority priority,
                 PolicySameVersionUpdate policy_same_version_update,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void,
                Update,
                (const std::string& app_id,
                 const std::string& install_data_index,
                 Priority priority,
                 PolicySameVersionUpdate policy_same_version_update,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void,
                UpdateAll,
                (base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void,
                Install,
                (const RegistrationRequest& registration,
                 const std::string& client_install_data,
                 const std::string& install_data_index,
                 Priority priority,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void, CancelInstalls, (const std::string& app_id), (override));
    MOCK_METHOD(void,
                RunInstaller,
                (const std::string& app_id,
                 const base::FilePath& installer_path,
                 const std::string& install_args,
                 const std::string& install_data,
                 const std::string& install_settings,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));

   protected:
    ~MockUpdateService() override = default;
  };

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support;

  scoped_refptr<MockUpdateService> mock_service =
      base::MakeRefCounted<MockUpdateService>();
  EXPECT_CALL(*mock_service, RegisterApp)
      .WillOnce([](const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) {
        VLOG(1) << "Client connected.";
        EXPECT_EQ(request.app_id, "org.chromium.KSAdminTest.Register");
        EXPECT_EQ(request.ap_key, "tag_key");
        EXPECT_EQ(request.ap_path, base::FilePath("tag_path"));
        EXPECT_EQ(request.version_key, "version_key");
        EXPECT_EQ(request.version_path, base::FilePath("version_path"));
        EXPECT_EQ(request.version, base::Version("1.2.3.4"));
        EXPECT_EQ(request.existence_checker_path, base::FilePath("/xc_path"));
        std::move(callback).Run(0);
      });

  // Create a stub and wait for the endpoint to be created before launching the
  // client process.
  base::RunLoop run_loop;
  auto service_stub = std::unique_ptr<UpdateServiceStub>(new UpdateServiceStub(
      mock_service, UpdaterScope::kUser, base::DoNothing(), base::DoNothing(),
      run_loop.QuitClosure()));
  run_loop.Run();

  base::RunLoop run_until_ksadmin_exit;
  base::Thread wait_thread("wait_for_ksadmin");
  wait_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  std::string out;
  wait_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&] {
        EXPECT_EQ(
            RunKSAdmin(&out, {"--register", "--version", "1.2.3.4", "--xcpath",
                              "/xc_path", "--tag-key", "tag_key", "--tag-path",
                              "tag_path", "--version-key", "version_key",
                              "--version-path", "version_path", "-P",
                              "org.chromium.KSAdminTest.Register"}),
            0);
      }),
      run_until_ksadmin_exit.QuitClosure());
  run_until_ksadmin_exit.Run();
  EXPECT_EQ(out, "");
}

}  // namespace updater

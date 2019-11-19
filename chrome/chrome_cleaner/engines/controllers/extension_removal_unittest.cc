// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade.h"
#include "chrome/chrome_cleaner/engines/controllers/main_controller.h"
#include "chrome/chrome_cleaner/engines/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/engines/target/test_engine_delegate.h"
#include "chrome/chrome_cleaner/ipc/mock_chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/mojom/test_mojo_sandbox_hooks.mojom.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/parsers/json_parser/sandboxed_json_parser.h"
#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner_impl.h"
#include "chrome/chrome_cleaner/scanner/mock_force_installed_extension_scanner.h"
#include "chrome/chrome_cleaner/test/scoped_file.h"
#include "chrome/chrome_cleaner/test/test_extensions.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/ui/silent_main_dialog.h"
#include "chrome/chrome_cleaner/zip_archiver/zip_archiver.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using testing::_;

class ExtensionTestSandboxHooks : public MojoSandboxSetupHooks {
 public:
  ExtensionTestSandboxHooks(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                            chrome_cleaner::EngineClient* engine_client)
      : mojo_task_runner_(mojo_task_runner), engine_client_(engine_client) {}

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override {
    mojo::ScopedMessagePipeHandle pipe_handle =
        SetupSandboxMessagePipe(policy, command_line);

    engine_client_->PostBindEngineCommandsRemote(std::move(pipe_handle));

    return RESULT_CODE_SUCCESS;
  }

 private:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  chrome_cleaner::EngineClient* engine_client_;
};

class TestMainController : public MainController {
 public:
  explicit TestMainController(ChromePromptIPC* chrome_prompt_ipc)
      : MainController(&test_rebooter_,
                       &test_registry_logger_,
                       chrome_prompt_ipc),
        test_registry_logger_(RegistryLogger::Mode::NOOP_FOR_TESTING),
        main_dialog_(std::make_unique<SilentMainDialog>(this)) {}

  MainDialogAPI* main_dialog() override { return main_dialog_.get(); }

  RegistryLogger test_registry_logger_;

  std::unique_ptr<MainDialogAPI> main_dialog_;

  class TestRebooter : public RebooterAPI {
   public:
    TestRebooter() {}
    void AppendPostRebootSwitch(const std::string& unused) override {}
    void AppendPostRebootSwitchASCII(const std::string& unused1,
                                     const std::string& unused2) override {}
    bool RegisterPostRebootRun(const base::CommandLine* current_command_line,
                               const std::string& cleanup_id,
                               ExecutionMode execution_mode,
                               bool logs_uploads_allowed) override {
      return true;
    }
    void UnregisterPostRebootRun() override {}
  } test_rebooter_;
};

class NoopZipArchiver : public ZipArchiver {
  void Archive(const base::FilePath& /*src_file_path*/,
               ArchiveResultCallback callback) override {
    std::move(callback).Run(mojom::ZipArchiverResultCode::kSuccess);
  }
};

base::FilePath CreateStartupDirectory() {
  base::FilePath start_menu_folder;
  CHECK(base::PathService::Get(base::DIR_START_MENU, &start_menu_folder));
  base::FilePath startup_dir = start_menu_folder.Append(L"Startup");
  CHECK(base::CreateDirectoryAndGetError(startup_dir, nullptr));

  return startup_dir;
}

}  // namespace

class ExtensionCleanupTest : public base::MultiProcessTest {
 public:
  ExtensionCleanupTest() : mojo_task_runner_(MojoTaskRunner::Create()) {}
  void SetUp() override {
    EXPECT_CALL(mock_chrome_prompt_ipc_, MockPostPromptUserTask(_, _, _, _))
        .WillRepeatedly([](const std::vector<base::FilePath>& files_to_delete,
                           const std::vector<base::string16>& registry_keys,
                           const std::vector<base::string16>& extension_ids,
                           ChromePromptIPC::PromptUserCallback* callback) {
          std::move(*callback).Run(PromptUserResponse::ACCEPTED_WITHOUT_LOGS);
        });
    EXPECT_CALL(mock_chrome_prompt_ipc_, Initialize(_));
    EXPECT_CALL(mock_chrome_prompt_ipc_, TryDeleteExtensions(_, _))
        .WillRepeatedly([](base::OnceClosure delete_allowed_callback,
                           base::OnceClosure delete_not_allowed_callback) {
          std::move(delete_allowed_callback).Run();
        });
    EXPECT_CALL(mock_settings_, execution_mode()).WillRepeatedly([]() {
      return ExecutionMode::kScanning;
    });
    EXPECT_CALL(mock_settings_, locations_to_scan())
        .WillRepeatedly(testing::ReturnRef(trace_locations_));
    Settings::SetInstanceForTesting(&mock_settings_);
    test_main_controller_ =
        std::make_unique<TestMainController>(&mock_chrome_prompt_ipc_);
    engine_client_ = SetupEngineClient();
    LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service_);

    // Set up fake windows registry
    registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE);
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);

    // setup fake file directories using |scoped_path_override_|
    base::FilePath program_files_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));
    fake_apps_dir_ = {
        program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps")};
    ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir_, nullptr));
    chrome_dir_ = {program_files_dir.Append(kChromeExePath)};
    ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir_, nullptr));

    test_pup_data_.AddPUP(kGoogleTestAUwSID, PUPData::FLAGS_ACTION_REMOVE,
                          "blah", 0);

    // Create some test UwS files.
    const base::FilePath startup_dir = CreateStartupDirectory();
    uws_A_ = ScopedFile::Create(startup_dir, chrome_cleaner::kTestUwsAFilename,
                                chrome_cleaner::kTestUwsAFileContents);
    uws_B_ = ScopedFile::Create(startup_dir, chrome_cleaner::kTestUwsBFilename,
                                chrome_cleaner::kTestUwsBFileContents);
  }

  void TearDown() override {
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
    Settings::SetInstanceForTesting(nullptr);
    engine_client_->ResetCreatedInstanceCheckForTesting();
  }

  void AddForcelistExtension(HKEY hkey,
                             const base::string16& name,
                             const base::string16& value) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.Create(hkey, kChromePoliciesForcelistKeyPath,
                                KEY_ALL_ACCESS));
    ASSERT_TRUE(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.WriteValue(name.c_str(), value.c_str()));
  }

  void AddExtensionSettingsExtension(HKEY hkey,
                                     const base::string16& name,
                                     const base::string16& value) {
    base::win::RegKey settings_key;
    ASSERT_EQ(ERROR_SUCCESS,
              settings_key.Create(hkey, kExtensionSettingsPolicyPath,
                                  KEY_ALL_ACCESS));
    ASSERT_TRUE(settings_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              settings_key.WriteValue(name.c_str(), value.c_str()));
  }

  // Note that this should only be called once, as subsequent calls will
  // overwrite whatever was there previously
  void SetDefaultExtensions(const std::string& extensions) {
    size_t extensions_size = extensions.size();
    base::FilePath default_extensions_file =
        fake_apps_dir_.Append(L"external_extensions.json");
    CreateFileWithContent(default_extensions_file, extensions.c_str(),
                          extensions_size);
    ASSERT_TRUE(base::PathExists(default_extensions_file));
  }

  // Note that this should only be called once, as subsequent calls will
  // overwrite whatever was there previously
  void SetMasterPreferencesExtensions(const std::string& extensions) {
    size_t extensions_size = extensions.size();
    base::FilePath master_preferences =
        chrome_dir_.Append(kMasterPreferencesFileName);
    CreateFileWithContent(master_preferences, extensions.c_str(),
                          extensions_size);
    ASSERT_TRUE(base::PathExists(master_preferences));
  }

  std::unique_ptr<chrome_cleaner::SandboxedJsonParser> SpawnSandboxProcesses(
      chrome_cleaner::EngineClient* engine_client) {
    ParserSandboxSetupHooks parser_setup_hooks(
        mojo_task_runner_.get(),
        base::BindOnce([] { FAIL() << "Parser sandbox disconnected"; }));
    ExtensionTestSandboxHooks engine_setup_hooks(mojo_task_runner_.get(),
                                                 engine_client);
    CHECK_EQ(RESULT_CODE_SUCCESS,
             StartSandboxTarget(MakeCmdLine("ParserSandboxMain"),
                                &parser_setup_hooks, SandboxType::kTest));

    CHECK_EQ(RESULT_CODE_SUCCESS,
             StartSandboxTarget(MakeCmdLine("EngineSandboxMain"),
                                &engine_setup_hooks, SandboxType::kTest));
    json_parser_ = std::make_unique<chrome_cleaner::RemoteParserPtr>(
        parser_setup_hooks.TakeParserRemote());
    return std::make_unique<chrome_cleaner::SandboxedJsonParser>(
        mojo_task_runner_.get(), json_parser_.get()->get());
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;

  testing::NiceMock<MockSettings> mock_settings_;

  std::unique_ptr<chrome_cleaner::EngineFacade> real_engine_facade_;
  std::unique_ptr<TestMainController> test_main_controller_;
  // Use StrictMock to be sure no unexpected IPC messages are sent.
  testing::StrictMock<MockChromePromptIPC> mock_chrome_prompt_ipc_;
  testing::NiceMock<MockLoggingService> mock_logging_service_;
  base::FilePath fake_apps_dir_;
  base::FilePath chrome_dir_;
  std::unique_ptr<chrome_cleaner::RemoteParserPtr> json_parser_;
  scoped_refptr<chrome_cleaner::EngineClient> engine_client_;
  chrome_cleaner::TestPUPData test_pup_data_;
  std::unique_ptr<ScopedFile> uws_A_;
  std::unique_ptr<ScopedFile> uws_B_;

 private:
  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedPathOverride program_files_override_{base::DIR_PROGRAM_FILES};
  const std::vector<UwS::TraceLocation> trace_locations_{
      chrome_cleaner::UwS_TraceLocation_FOUND_IN_SHELL};
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<EngineClient> SetupEngineClient() {
    SandboxConnectionErrorCallback connection_error_callback =
        base::BindRepeating(&ExtensionCleanupTest::SandboxErrorCallback,
                            base::Unretained(this));
    scoped_refptr<EngineClient> client = EngineClient::CreateEngineClient(
        Engine::TEST_ONLY, base::DoNothing::Repeatedly<int>(),
        std::move(connection_error_callback), mojo_task_runner_.get());
    client->archiver_for_testing_ = std::make_unique<NoopZipArchiver>();
    return client;
  }

  void SandboxErrorCallback(SandboxType type) {
    LOG(ERROR) << "Sandbox failed";
    ASSERT_TRUE(false);
  }
};

std::unique_ptr<UwEMatchers> MakeMatchersFromExtensions(
    const std::vector<ForceInstalledExtension>& extensions) {
  std::unique_ptr<UwEMatchers> matchers = std::make_unique<UwEMatchers>();
  for (const ForceInstalledExtension& extension : extensions) {
    UwEMatcher matcher;
    matcher.add_uws_id(kGoogleTestAUwSID);
    UwEMatcher::MatcherCriteria criteria;
    criteria.add_extension_id(extension.id.AsString());
    criteria.add_install_method(static_cast<UwEMatcher_ExtensionInstallMethod>(
        extension.install_method));
    *matcher.mutable_criteria() = criteria;
    *matchers->add_uwe_matcher() = matcher;
  }
  return matchers;
}

MULTIPROCESS_TEST_MAIN(ParserSandboxMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  return chrome_cleaner::RunParserSandboxTarget(
      *base::CommandLine::ForCurrentProcess(), sandbox_target_services);
}

MULTIPROCESS_TEST_MAIN(EngineSandboxMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);
  return RunEngineSandboxTarget(
      base::MakeRefCounted<chrome_cleaner::TestEngineDelegate>(),
      *base::CommandLine::ForCurrentProcess(), sandbox_target_services);
}

TEST_F(ExtensionCleanupTest, CleanupExtensions) {
  std::vector<ForceInstalledExtension> extensions_to_cleanup{
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId1)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId2)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId5)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId4)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId3)).value(),
       POLICY_EXTENSION_FORCELIST},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId6)).value(),
       POLICY_MASTER_PREFERENCES},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId7)).value(),
       POLICY_MASTER_PREFERENCES},
  };
  std::unique_ptr<UwEMatchers> matchers =
      MakeMatchersFromExtensions(extensions_to_cleanup);

  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    AddForcelistExtension(policy.hkey, policy.name, policy.value);
  }

  AddExtensionSettingsExtension(HKEY_LOCAL_MACHINE, kExtensionSettingsName,
                                kExtensionSettingsJsonOnlyForced);

  SetDefaultExtensions(kDefaultExtensionsJson);

  SetMasterPreferencesExtensions(kMasterPreferencesJson);

  std::unique_ptr<chrome_cleaner::SandboxedJsonParser> json_parser =
      SpawnSandboxProcesses(engine_client_.get());
  engine_client_->Initialize();

  ForceInstalledExtensionScannerImpl real_scanner;
  std::vector<ForceInstalledExtension> actual_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(extensions_to_cleanup,
              testing::UnorderedElementsAreArray(actual_extensions));
  std::unique_ptr<MockForceInstalledExtensionScanner> mock_scanner =
      std::make_unique<MockForceInstalledExtensionScanner>();
  EXPECT_CALL(*mock_scanner, GetForceInstalledExtensions(_))
      .WillOnce(testing::Return(actual_extensions));
  EXPECT_CALL(*mock_scanner, CreateUwEMatchersFromResource(_))
      .WillOnce((testing::Return(testing::ByMove(std::move(matchers)))));
  real_engine_facade_ = std::make_unique<chrome_cleaner::EngineFacade>(
      engine_client_, json_parser.get(), test_main_controller_->main_dialog(),
      std::move(mock_scanner), &mock_chrome_prompt_ipc_);

  test_main_controller_->SetEngineFacade(real_engine_facade_.get());
  ON_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillByDefault(testing::Return(true));
  ASSERT_EQ(test_main_controller_->ScanAndClean(),
            chrome_cleaner::RESULT_CODE_SUCCESS);

  std::vector<ForceInstalledExtension> empty;
  std::vector<ForceInstalledExtension> found_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(found_extensions, testing::ContainerEq(empty));
}

TEST_F(ExtensionCleanupTest, CleanupSomeExtensions) {
  std::vector<ForceInstalledExtension> expected_extensions{
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId1)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId2)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId3)).value(),
       POLICY_EXTENSION_FORCELIST},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId4)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId5)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId6)).value(),
       POLICY_MASTER_PREFERENCES},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId7)).value(),
       POLICY_MASTER_PREFERENCES},
  };
  std::vector<ForceInstalledExtension> expected_final_extensions{
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId1)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId2)).value(),
       DEFAULT_APPS_EXTENSION}};
  std::vector<ForceInstalledExtension> extensions_to_cleanup{
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId3)).value(),
       POLICY_EXTENSION_FORCELIST},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId4)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId5)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId6)).value(),
       POLICY_MASTER_PREFERENCES},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId7)).value(),
       POLICY_MASTER_PREFERENCES},
  };
  std::unique_ptr<UwEMatchers> matchers =
      MakeMatchersFromExtensions(extensions_to_cleanup);

  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    AddForcelistExtension(policy.hkey, policy.name, policy.value);
  }

  AddExtensionSettingsExtension(HKEY_LOCAL_MACHINE, kExtensionSettingsName,
                                kExtensionSettingsJsonOnlyForced);

  SetDefaultExtensions(kDefaultExtensionsJson);

  SetMasterPreferencesExtensions(kMasterPreferencesJson);

  std::unique_ptr<chrome_cleaner::SandboxedJsonParser> json_parser =
      SpawnSandboxProcesses(engine_client_.get());
  engine_client_->Initialize();

  ForceInstalledExtensionScannerImpl real_scanner;
  std::vector<ForceInstalledExtension> actual_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(expected_extensions,
              testing::UnorderedElementsAreArray(actual_extensions));
  std::unique_ptr<MockForceInstalledExtensionScanner> mock_scanner =
      std::make_unique<MockForceInstalledExtensionScanner>();
  EXPECT_CALL(*mock_scanner, GetForceInstalledExtensions(_))
      .WillOnce(testing::Return(actual_extensions));
  EXPECT_CALL(*mock_scanner, CreateUwEMatchersFromResource(_))
      .WillOnce((testing::Return(testing::ByMove(std::move(matchers)))));
  real_engine_facade_ = std::make_unique<chrome_cleaner::EngineFacade>(
      engine_client_, json_parser.get(), test_main_controller_->main_dialog(),
      std::move(mock_scanner), &mock_chrome_prompt_ipc_);

  test_main_controller_->SetEngineFacade(real_engine_facade_.get());
  ON_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillByDefault(testing::Return(true));
  ASSERT_EQ(test_main_controller_->ScanAndClean(),
            chrome_cleaner::RESULT_CODE_SUCCESS);

  std::vector<ForceInstalledExtension> found_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(found_extensions,
              testing::UnorderedElementsAreArray(expected_final_extensions));
}

TEST_F(ExtensionCleanupTest, CleanupNoExtensions) {
  std::vector<ForceInstalledExtension> expected_extensions{
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId1)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId2)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId3)).value(),
       POLICY_EXTENSION_FORCELIST},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId4)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId5)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId6)).value(),
       POLICY_MASTER_PREFERENCES},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId7)).value(),
       POLICY_MASTER_PREFERENCES},
  };
  std::vector<ForceInstalledExtension> extensions_to_cleanup;
  std::unique_ptr<UwEMatchers> matchers =
      MakeMatchersFromExtensions(extensions_to_cleanup);

  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    AddForcelistExtension(policy.hkey, policy.name, policy.value);
  }

  AddExtensionSettingsExtension(HKEY_LOCAL_MACHINE, kExtensionSettingsName,
                                kExtensionSettingsJsonOnlyForced);

  SetDefaultExtensions(kDefaultExtensionsJson);

  SetMasterPreferencesExtensions(kMasterPreferencesJson);

  std::unique_ptr<chrome_cleaner::SandboxedJsonParser> json_parser =
      SpawnSandboxProcesses(engine_client_.get());
  engine_client_->Initialize();

  ForceInstalledExtensionScannerImpl real_scanner;
  std::vector<ForceInstalledExtension> actual_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(expected_extensions,
              testing::UnorderedElementsAreArray(actual_extensions));
  std::unique_ptr<MockForceInstalledExtensionScanner> mock_scanner =
      std::make_unique<MockForceInstalledExtensionScanner>();
  EXPECT_CALL(*mock_scanner, GetForceInstalledExtensions(_))
      .WillOnce(testing::Return(actual_extensions));
  EXPECT_CALL(*mock_scanner, CreateUwEMatchersFromResource(_))
      .WillOnce((testing::Return(testing::ByMove(std::move(matchers)))));
  real_engine_facade_ = std::make_unique<chrome_cleaner::EngineFacade>(
      engine_client_, json_parser.get(), test_main_controller_->main_dialog(),
      std::move(mock_scanner), &mock_chrome_prompt_ipc_);

  test_main_controller_->SetEngineFacade(real_engine_facade_.get());
  ON_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillByDefault(testing::Return(true));
  ASSERT_EQ(test_main_controller_->ScanAndClean(),
            chrome_cleaner::RESULT_CODE_SUCCESS);

  std::vector<ForceInstalledExtension> found_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(found_extensions,
              testing::UnorderedElementsAreArray(expected_extensions));
}

TEST_F(ExtensionCleanupTest, DISABLED_CleanupNoExtensionsWhenNotAllowed) {
  std::vector<ForceInstalledExtension> expected_extensions{
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId1)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId2)).value(),
       DEFAULT_APPS_EXTENSION},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId3)).value(),
       POLICY_EXTENSION_FORCELIST},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId4)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId5)).value(),
       POLICY_EXTENSION_SETTINGS},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId6)).value(),
       POLICY_MASTER_PREFERENCES},
      {ExtensionID::Create(base::UTF16ToUTF8(kTestExtensionId7)).value(),
       POLICY_MASTER_PREFERENCES},
  };
  std::vector<ForceInstalledExtension> extensions_to_cleanup =
      expected_extensions;
  std::unique_ptr<UwEMatchers> matchers =
      MakeMatchersFromExtensions(extensions_to_cleanup);

  for (const TestRegistryEntry& policy : kExtensionForcelistEntries) {
    AddForcelistExtension(policy.hkey, policy.name, policy.value);
  }

  AddExtensionSettingsExtension(HKEY_LOCAL_MACHINE, kExtensionSettingsName,
                                kExtensionSettingsJsonOnlyForced);

  SetDefaultExtensions(kDefaultExtensionsJson);

  SetMasterPreferencesExtensions(kMasterPreferencesJson);

  std::unique_ptr<chrome_cleaner::SandboxedJsonParser> json_parser =
      SpawnSandboxProcesses(engine_client_.get());
  engine_client_->Initialize();

  ForceInstalledExtensionScannerImpl real_scanner;
  std::vector<ForceInstalledExtension> actual_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(expected_extensions,
              testing::UnorderedElementsAreArray(actual_extensions));
  std::unique_ptr<MockForceInstalledExtensionScanner> mock_scanner =
      std::make_unique<MockForceInstalledExtensionScanner>();
  EXPECT_CALL(*mock_scanner, GetForceInstalledExtensions(_))
      .WillOnce(testing::Return(actual_extensions));
  EXPECT_CALL(*mock_scanner, CreateUwEMatchersFromResource(_))
      .WillOnce((testing::Return(testing::ByMove(std::move(matchers)))));

  EXPECT_CALL(mock_chrome_prompt_ipc_, TryDeleteExtensions(_, _))
      .WillOnce([](base::OnceClosure delete_allowed_callback,
                   base::OnceClosure delete_not_allowed_callback) {
        std::move(delete_not_allowed_callback).Run();
      });

  real_engine_facade_ = std::make_unique<chrome_cleaner::EngineFacade>(
      engine_client_, json_parser.get(), test_main_controller_->main_dialog(),
      std::move(mock_scanner), &mock_chrome_prompt_ipc_);

  test_main_controller_->SetEngineFacade(real_engine_facade_.get());
  ON_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillByDefault(testing::Return(true));
  ASSERT_EQ(test_main_controller_->ScanAndClean(),
            chrome_cleaner::RESULT_CODE_SUCCESS);

  std::vector<ForceInstalledExtension> found_extensions =
      real_scanner.GetForceInstalledExtensions(json_parser.get());
  EXPECT_THAT(found_extensions,
              testing::UnorderedElementsAreArray(expected_extensions));
}

}  // namespace chrome_cleaner
